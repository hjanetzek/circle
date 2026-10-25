//
// nicontroller.cpp
//
#include <nicontrol/nicontroller.h>
#include <nicontrol/nikkm32.h>
#include <circle/usb/usbdevicefactory.h>
#include <circle/usb/usbhid.h>
#include <circle/usb/usbhostcontroller.h>
#include <circle/usb/usbdevice.h>
#include <circle/usb/usbstring.h>
#include <circle/devicenameservice.h>
#include <circle/synchronize.h>
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

static const char From[] = "nictl";
static const char DevicePrefix[] = "nictl";

static const unsigned MaxReportSize = 64;

CNumberPool CNIControllerDevice::s_DeviceNumberPool (1);

CNIControllerDevice::CNIControllerDevice (CUSBFunction *pFunction, unsigned nLEDs,
					  unsigned nDisplayWidth, unsigned nDisplayHeight)
:	CUSBHIDDevice (pFunction, MaxReportSize),
	m_nDeviceNumber (0),
	m_nEventIn (0),
	m_nEventOut (0),
	m_nLEDs (nLEDs),
	m_bLEDsChanged (TRUE),
	m_nDisplayWidth (nDisplayWidth),
	m_nDisplayHeight (nDisplayHeight),
	m_pDisplay (0),
	m_bDisplayChanged (FALSE),
	m_bUseEndpointOut (TRUE)
{
	assert (m_nLEDs <= MaxLEDs);
	memset (m_LED, 0, sizeof m_LED);

	if (m_nDisplayWidth > 0)
	{
		m_pDisplay = new u8[m_nDisplayWidth * ((m_nDisplayHeight + 7) / 8)];
		ClearDisplay ();
		m_bDisplayChanged = FALSE;
	}
}

CNIControllerDevice::~CNIControllerDevice (void)
{
	if (m_nDeviceNumber != 0)
	{
		CDeviceNameService::Get ()->RemoveDevice (DevicePrefix, m_nDeviceNumber, FALSE);
		s_DeviceNumberPool.FreeNumber (m_nDeviceNumber);
		m_nDeviceNumber = 0;
	}

	delete [] m_pDisplay;
	m_pDisplay = 0;
}

boolean CNIControllerDevice::Configure (void)
{
	if (!ConfigureHID (MaxReportSize))
	{
		CLogger::Get ()->Write (From, LogError, "Cannot configure HID device");

		return FALSE;
	}

	m_nDeviceNumber = s_DeviceNumberPool.AllocateNumber (TRUE, From);
	CDeviceNameService::Get ()->AddDevice (DevicePrefix, m_nDeviceNumber, this, FALSE);

	CLogger::Get ()->Write (From, LogNotice, "%s%u: %s", DevicePrefix, m_nDeviceNumber,
				GetModelName ());

	return StartRequest ();
}

boolean CNIControllerDevice::GetEvent (TNIEvent *pEvent)
{
	assert (pEvent != 0);

	unsigned nOut = m_nEventOut;
	if (nOut == m_nEventIn)
	{
		return FALSE;
	}

	DataMemBarrier ();
	*pEvent = m_Events[nOut];
	DataMemBarrier ();
	m_nEventOut = (nOut + 1) & (EventQueueSize - 1);

	return TRUE;
}

void CNIControllerDevice::SetLED (unsigned nLED, u8 ucBrightness)
{
	if (   nLED < m_nLEDs
	    && m_LED[nLED] != ucBrightness)
	{
		m_LED[nLED] = ucBrightness;
		m_bLEDsChanged = TRUE;
	}
}

void CNIControllerDevice::ClearDisplay (void)
{
	if (m_pDisplay != 0)
	{
		memset (m_pDisplay, 0, m_nDisplayWidth * ((m_nDisplayHeight + 7) / 8));
		m_bDisplayChanged = TRUE;
	}
}

void CNIControllerDevice::SetPixel (unsigned x, unsigned y, boolean bOn)
{
	if (   m_pDisplay == 0
	    || x >= m_nDisplayWidth
	    || y >= m_nDisplayHeight)
	{
		return;
	}

	u8 *pByte = &m_pDisplay[(y / 8) * m_nDisplayWidth + x];
	u8 ucMask = 1 << (y % 8);
	u8 ucNew = bOn ? *pByte | ucMask : *pByte & ~ucMask;
	if (ucNew != *pByte)
	{
		*pByte = ucNew;
		m_bDisplayChanged = TRUE;
	}
}

void CNIControllerDevice::Update (void)
{
	if (m_bLEDsChanged)
	{
		m_bLEDsChanged = !SendLEDs (m_LED);
	}

	if (m_bDisplayChanged)
	{
		m_bDisplayChanged = !SendDisplay (m_pDisplay);
	}
}

void CNIControllerDevice::RegisterDriver (void)
{
	CUSBDeviceFactory::RegisterFactory (Factory);
}

void CNIControllerDevice::PostEvent (TNIEventType Type, unsigned nControl, int nValue)
{
	unsigned nIn = m_nEventIn;
	unsigned nNext = (nIn + 1) & (EventQueueSize - 1);
	if (nNext == m_nEventOut)
	{
		return;			// full, the event is lost
	}

	m_Events[nIn].Type = Type;
	m_Events[nIn].nControl = nControl;
	m_Events[nIn].nValue = nValue;
	DataMemBarrier ();
	m_nEventIn = nNext;
}

boolean CNIControllerDevice::SendReport (const u8 *pReport, unsigned nLength)
{
	assert (pReport != 0);
	assert (nLength > 1);

	// synchronous: the xHCI driver allows one pending transfer per endpoint
	if (m_bUseEndpointOut)
	{
		if (SendToEndpointOut (pReport, nLength))	// xHCI: no timeouts
		{
			return TRUE;
		}

		m_bUseEndpointOut = FALSE;	// no interrupt OUT endpoint
	}

	return GetHost ()->ControlMessage (GetEndpoint0 (),
					   REQUEST_OUT | REQUEST_CLASS | REQUEST_TO_INTERFACE,
					   SET_REPORT, (REPORT_TYPE_OUTPUT << 8) | pReport[0],
					   GetInterfaceNumber (), (void *) pReport, nLength) >= 0;
}

void CNIControllerDevice::ReportHandler (const u8 *pReport, unsigned nReportSize)
{
	if (   pReport != 0
	    && nReportSize > 0)
	{
		DecodeReport (pReport, nReportSize);
	}
}

// the manufacturer string is "Native Instruments" (open firmware keeps the IDs)
boolean CNIControllerDevice::IsStockFirmware (CUSBDevice *pDevice)
{
	const TUSBDeviceDescriptor *pDesc = pDevice->GetDeviceDescriptor ();
	if (   pDesc == 0
	    || pDesc->iManufacturer == 0)
	{
		return FALSE;
	}

	CUSBString String (pDevice);
	return    String.GetFromDescriptor (pDesc->iManufacturer, String.GetLanguageID ())
	       && strcmp (String.Get (), "Native Instruments") == 0;
}

CUSBFunction *CNIControllerDevice::Factory (CUSBFunction *pParent, CString *pName)
{
	if (pName->Compare ("int3-0-0") != 0)
	{
		return 0;
	}

	CString *pVendor = pParent->GetDevice ()->GetName (DeviceNameVendor);
	assert (pVendor != 0);

	CUSBFunction *pResult = 0;
	if (   pVendor->Compare ("ven17cc-1860") == 0
	    && IsStockFirmware (pParent->GetDevice ()))	// not e.g. My Kontrol
	{
		pResult = new CNIKompleteKontrolM32 (pParent);
	}

	delete pVendor;

	return pResult;
}
