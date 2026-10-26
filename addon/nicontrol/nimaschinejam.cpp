//
// nimaschinejam.cpp
//
#include <nicontrol/nimaschinejam.h>
#include <circle/util.h>
#include <assert.h>

CNIMaschineJam::CNIMaschineJam (CUSBFunction *pFunction)
:	CNIControllerDevice (pFunction, LEDCount, 0, 0),
	m_nEncoder (-1),
	m_bRGBChanged (TRUE),
	m_bStripLEDsChanged (TRUE)
{
	memset (m_Buttons, 0, sizeof m_Buttons);
	memset (m_nStrip, 0, sizeof m_nStrip);
	memset (m_RGB, 0, sizeof m_RGB);
	memset (m_StripLED, 0, sizeof m_StripLED);
}

void CNIMaschineJam::SetPad (unsigned nPad, u8 ucColour)
{
	if (nPad < PadCount)
	{
		SetRGB (8 + nPad, ucColour);
	}
}

void CNIMaschineJam::SetScene (unsigned nScene, u8 ucColour)
{
	if (nScene < 8)
	{
		SetRGB (nScene, ucColour);
	}
}

void CNIMaschineJam::SetGroup (unsigned nGroup, u8 ucColour)
{
	if (nGroup < 8)
	{
		SetRGB (72 + nGroup, ucColour);
	}
}

void CNIMaschineJam::SetStripLED (unsigned nStrip, unsigned nLED, u8 ucColour)
{
	if (   nStrip < StripCount
	    && nLED < StripLEDs)
	{
		u8 *pLED = &m_StripLED[nStrip * StripLEDs + nLED];
		if (*pLED != ucColour)
		{
			*pLED = ucColour;
			m_bStripLEDsChanged = TRUE;
		}
	}
}

void CNIMaschineJam::SetStripBar (unsigned nStrip, unsigned nLEDs, u8 ucColour)
{
	for (unsigned i = 0; i < StripLEDs; i++)
	{
		SetStripLED (nStrip, i, i < nLEDs ? ucColour : 0);
	}
}

void CNIMaschineJam::SetRGB (unsigned nIndex, u8 ucColour)
{
	assert (nIndex < RGBLEDs);
	if (m_RGB[nIndex] != ucColour)
	{
		m_RGB[nIndex] = ucColour;
		m_bRGBChanged = TRUE;
	}
}

void CNIMaschineJam::DecodeReport (const u8 *pReport, unsigned nLength)
{
	switch (pReport[0])
	{
	case 1:
		DecodeButtons (pReport, nLength);
		break;

	case 2:
		DecodeStrips (pReport, nLength);
		break;

	default:
		break;
	}
}

// report 1: [0] ID, [1] encoder (low nibble), [2..16] button bits (LSB first)
void CNIMaschineJam::DecodeButtons (const u8 *pReport, unsigned nLength)
{
	if (nLength < 2 + ButtonBytes)
	{
		return;
	}

	// all changes of the report, not just the first one
	for (unsigned i = 0; i < ButtonBytes; i++)
	{
		u8 ucChanged = pReport[2 + i] ^ m_Buttons[i];
		for (unsigned nBit = 0; ucChanged; nBit++, ucChanged >>= 1)
		{
			if (   (ucChanged & 1)
			    && i * 8 + nBit < ButtonCount)
			{
				PostEvent (NIEventButton, i * 8 + nBit, (pReport[2 + i] >> nBit) & 1);
			}
		}

		m_Buttons[i] = pReport[2 + i];
	}

	int nPosition = pReport[1] & 0x0F;
	if (   m_nEncoder >= 0
	    && nPosition != m_nEncoder)
	{
		int nDelta = (nPosition - m_nEncoder) & 0x0F;	// 4 bit counter
		if (nDelta >= 8)
		{
			nDelta -= 16;
		}

		PostEvent (NIEventEncoder, EncoderMain, nDelta);
	}
	m_nEncoder = nPosition;
}

// report 2: [0] ID, then per strip: time, finger 1, finger 2 (u16 LE)
void CNIMaschineJam::DecodeStrips (const u8 *pReport, unsigned nLength)
{
	if (nLength < 1 + StripCount * 6)
	{
		return;
	}

	for (unsigned i = 0; i < StripCount; i++)
	{
		const u8 *p = pReport + 1 + i * 6;
		int nPosition = p[2] | (p[3] << 8);
		if (nPosition != m_nStrip[i])
		{
			m_nStrip[i] = nPosition;
			PostEvent (NIEventStrip, i, nPosition != 0 ? nPosition : -1);
		}
	}
}

boolean CNIMaschineJam::SendLEDs (const u8 *pBrightness)
{
	u8 Report[1 + LEDCount];
	Report[0] = 0x80;
	memcpy (Report + 1, pBrightness, LEDCount);

	return SendReport (Report, sizeof Report);
}

void CNIMaschineJam::UpdateModel (void)
{
	if (m_bRGBChanged)
	{
		u8 Report[1 + RGBLEDs];
		Report[0] = 0x81;
		memcpy (Report + 1, m_RGB, RGBLEDs);

		m_bRGBChanged = !SendReport (Report, sizeof Report);
	}

	if (m_bStripLEDsChanged)
	{
		u8 Report[1 + sizeof m_StripLED];
		Report[0] = 0x82;
		memcpy (Report + 1, m_StripLED, sizeof m_StripLED);

		m_bStripLEDsChanged = !SendReport (Report, sizeof Report);
	}
}
