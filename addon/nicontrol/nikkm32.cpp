//
// nikkm32.cpp
//
#include <nicontrol/nikkm32.h>
#include <circle/util.h>
#include <assert.h>

static const unsigned DisplayWidth = 128;
static const unsigned DisplayHeight = 32;

CNIKompleteKontrolM32::CNIKompleteKontrolM32 (CUSBFunction *pFunction)
:	CNIControllerDevice (pFunction, LEDCount, DisplayWidth, DisplayHeight),
	m_nJoyPosition (-1)
{
	memset (m_Buttons, 0, sizeof m_Buttons);
}

// report 1: [0] ID, [1..5] button bits (LSB first), [6..21] knobs (unused in
// MIDI mode), [24] joycoder position in the low nibble, [37] lowest key
void CNIKompleteKontrolM32::DecodeReport (const u8 *pReport, unsigned nLength)
{
	if (   pReport[0] != 1
	    || nLength < 25)
	{
		return;
	}

	for (unsigned i = 0; i < ButtonBytes; i++)
	{
		u8 ucChanged = pReport[1 + i] ^ m_Buttons[i];
		for (unsigned nBit = 0; ucChanged; nBit++, ucChanged >>= 1)
		{
			if (ucChanged & 1)
			{
				PostEvent (NIEventButton, i * 8 + nBit, (pReport[1 + i] >> nBit) & 1);
			}
		}

		m_Buttons[i] = pReport[1 + i];
	}

	int nPosition = pReport[24] & 0x0F;
	if (   m_nJoyPosition >= 0
	    && nPosition != m_nJoyPosition)
	{
		int nDelta = (nPosition - m_nJoyPosition) & 0x0F;	// 4 bit counter
		if (nDelta >= 8)
		{
			nDelta -= 16;
		}

		PostEvent (NIEventEncoder, EncoderJoy, nDelta);
	}
	m_nJoyPosition = nPosition;
}

boolean CNIKompleteKontrolM32::SendLEDs (const u8 *pBrightness)
{
	u8 Report[1 + LEDCount];
	Report[0] = 0x80;
	memcpy (Report + 1, pBrightness, LEDCount);

	return SendReport (Report, sizeof Report);
}

// two reports of 2 pages (16 rows): header x, page, width, pages (u16 LE),
// then [page][x]; a set bit is a dark pixel
boolean CNIKompleteKontrolM32::SendDisplay (const u8 *pPages)
{
	for (unsigned nPage = 0; nPage < DisplayHeight / 8; nPage += 2)
	{
		u8 Report[1 + 8 + 2 * DisplayWidth];
		Report[0] = 0xE0;
		const u16 Header[4] = {0, (u16) nPage, DisplayWidth, 2};
		for (unsigned i = 0; i < 4; i++)
		{
			Report[1 + 2*i] = Header[i] & 0xFF;
			Report[2 + 2*i] = Header[i] >> 8;
		}

		for (unsigned i = 0; i < 2 * DisplayWidth; i++)
		{
			Report[9 + i] = ~pPages[nPage * DisplayWidth + i];
		}

		if (!SendReport (Report, sizeof Report))
		{
			return FALSE;
		}
	}

	return TRUE;
}
