//
// nicontroller.h
//
// Driver for the HID interface of Native Instruments controllers (stock
// firmware, no NI software needed): buttons and encoders as events, button
// LEDs and a monochrome display. Keys, knobs and strips of keyboards come
// over their USB MIDI interface, which Circle's USB MIDI driver handles.
//
// Supported: Komplete Kontrol M32 (17cc:1860).
//
// Call CNIControllerDevice::RegisterDriver() before the USB host controller
// is initialized. A connected controller is "nictlN" in the device name
// service.
//
#ifndef _nicontrol_nicontroller_h
#define _nicontrol_nicontroller_h

#include <circle/usb/usbhiddevice.h>
#include <circle/usb/usbfunction.h>
#include <circle/numberpool.h>
#include <circle/string.h>
#include <circle/types.h>

enum TNIEventType
{
	NIEventButton,		// nControl: model specific button, nValue: 1 pressed, 0 released
	NIEventEncoder		// nControl: model specific encoder, nValue: steps (signed)
};

struct TNIEvent
{
	u8 Type;		// TNIEventType
	u8 nControl;
	s16 nValue;
};

class CNIControllerDevice : public CUSBHIDDevice
{
public:
	enum TModel
	{
		ModelKompleteKontrolM32
	};

public:
	CNIControllerDevice (CUSBFunction *pFunction, unsigned nLEDs,
			     unsigned nDisplayWidth, unsigned nDisplayHeight);
	~CNIControllerDevice (void);

	boolean Configure (void);

	virtual TModel GetModel (void) const = 0;
	virtual const char *GetModelName (void) const = 0;

	unsigned GetDeviceNumber (void) const		{ return m_nDeviceNumber; }

	// next input event, FALSE if none (task level)
	boolean GetEvent (TNIEvent *pEvent);

	// button LEDs, brightness 0..127
	unsigned GetLEDCount (void) const		{ return m_nLEDs; }
	void SetLED (unsigned nLED, u8 ucBrightness);

	// display, 1 bit per pixel, bit set = lit
	unsigned GetDisplayWidth (void) const		{ return m_nDisplayWidth; }
	unsigned GetDisplayHeight (void) const		{ return m_nDisplayHeight; }
	void ClearDisplay (void);
	void SetPixel (unsigned x, unsigned y, boolean bOn);

	// sends changed LEDs and display contents (task level, not from an
	// event handler of the USB stack)
	void Update (void);

	// the factory for CUSBDeviceFactory::RegisterFactory()
	static void RegisterDriver (void);

protected:
	// the model's report layout
	virtual void DecodeReport (const u8 *pReport, unsigned nLength) = 0;
	virtual boolean SendLEDs (const u8 *pBrightness) = 0;
	virtual boolean SendDisplay (const u8 *pPages) = 0;	// [page][x], bit 0 = top row

	void PostEvent (TNIEventType Type, unsigned nControl, int nValue);	// from DecodeReport()

	// an output report (first byte = report ID), synchronous: interrupt OUT
	// endpoint if there is one, otherwise SET_REPORT on endpoint 0
	boolean SendReport (const u8 *pReport, unsigned nLength);

private:
	void ReportHandler (const u8 *pReport, unsigned nReportSize) override;

	static CUSBFunction *Factory (CUSBFunction *pParent, CString *pName);

private:
	unsigned m_nDeviceNumber;

	static const unsigned EventQueueSize = 128;	// power of 2
	TNIEvent m_Events[EventQueueSize];
	volatile unsigned m_nEventIn;
	volatile unsigned m_nEventOut;

	unsigned m_nLEDs;
	static const unsigned MaxLEDs = 64;
	u8 m_LED[MaxLEDs];
	boolean m_bLEDsChanged;

	unsigned m_nDisplayWidth;
	unsigned m_nDisplayHeight;
	u8 *m_pDisplay;			// [page][x]
	boolean m_bDisplayChanged;

	boolean m_bUseEndpointOut;

	static CNumberPool s_DeviceNumberPool;
};

#endif
