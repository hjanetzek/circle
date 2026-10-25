//
// mykontrol.cpp
//
#include <nicontrol/mykontrol.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/synchronize.h>
#include <circle/util.h>
#include <assert.h>

static const char From[] = "mykontrol";

static const u8 Cable = 1;		// "DAW"
static const u8 ControlChannel = 15;	// MIDI channel 16

static const unsigned KeepaliveMs = 500;
static const unsigned HelloRetryMs = 2000;

enum
{
	CmdHello = 0x00, CmdIdent = 0x01, CmdHi = 0x02, CmdPing = 0x03, CmdBye = 0x04,
	CmdView = 0x05, CmdRefresh = 0x06, CmdModes = 0x09, CmdSetMode = 0x0A,
	CmdText = 0x10, CmdLight = 0x12, CmdTransport = 0x13, CmdNotify = 0x15,
	CmdPage = 0x20, CmdSlot = 0x21, CmdValue = 0x22
};

static const u8 Header[] = {0x7D, 0x4D, 0x4B};

CMyKontrolHost::CMyKontrolHost (const char *pName, const char *pVersion, unsigned nFeatures)
:	m_pName (pName),
	m_pVersion (pVersion),
	m_nFeatures (nFeatures),
	m_pSendHandler (0),
	m_pSendParam (0),
	m_nRxIn (0),
	m_nRxOut (0),
	m_nSysExLength (0),
	m_bInSysEx (FALSE),
	m_nEventIn (0),
	m_nEventOut (0),
	m_bConnected (FALSE),
	m_bHelloPending (TRUE),
	m_nMode (ModeInstrument),
	m_bShift (FALSE),
	m_ucGen (0),
	m_nLastSendTicks (0),
	m_nLastHelloTicks (0)
{
}

void CMyKontrolHost::SetSendHandler (TSendHandler *pHandler, void *pParam)
{
	m_pSendParam = pParam;
	m_pSendHandler = pHandler;
}

void CMyKontrolHost::Reset (void)
{
	m_bConnected = FALSE;
	m_bHelloPending = TRUE;
	m_bInSysEx = FALSE;
	m_bShift = FALSE;
	m_nRxOut = m_nRxIn;
}

void CMyKontrolHost::ReceivePacket (const u8 *pPacket)
{
	unsigned nIn = m_nRxIn;
	unsigned nNext = (nIn + 1) & (RxQueueSize - 1);
	if (nNext == m_nRxOut)
	{
		return;			// full
	}

	memcpy (m_RxQueue[nIn], pPacket, 4);
	DataMemBarrier ();
	m_nRxIn = nNext;
}

void CMyKontrolHost::Process (void)
{
	while (m_nRxOut != m_nRxIn)
	{
		DataMemBarrier ();
		const u8 *pPacket = m_RxQueue[m_nRxOut];

		u8 ucCIN = pPacket[0] & 0x0F;
		unsigned nBytes = 0;
		switch (ucCIN)
		{
		case 0x4:	nBytes = 3;	break;	// sysex start or continue
		case 0x5:	nBytes = 1;	break;	// sysex end (or 1 byte common)
		case 0x6:	nBytes = 2;	break;
		case 0x7:	nBytes = 3;	break;
		default:
			if (ucCIN >= 0x8 && ucCIN <= 0xE)
			{
				HandleChannelMessage (pPacket + 1);
			}
			break;
		}

		for (unsigned i = 0; i < nBytes; i++)
		{
			u8 ucByte = pPacket[1 + i];
			if (ucByte == 0xF0)
			{
				m_bInSysEx = TRUE;
				m_nSysExLength = 0;
			}
			else if (ucByte == 0xF7)
			{
				if (m_bInSysEx)
				{
					HandleSysEx (m_SysEx, m_nSysExLength);
				}
				m_bInSysEx = FALSE;
			}
			else if (   m_bInSysEx
				 && m_nSysExLength < MaxSysEx)
			{
				m_SysEx[m_nSysExLength++] = ucByte;
			}
		}

		DataMemBarrier ();
		m_nRxOut = (m_nRxOut + 1) & (RxQueueSize - 1);
	}

	unsigned nTicks = CTimer::Get ()->GetClockTicks ();

	if (   !m_bConnected
	    && (   m_bHelloPending
		|| nTicks - m_nLastHelloTicks >= HelloRetryMs * 1000))
	{
		SendHello ();
	}

	if (   m_bConnected
	    && nTicks - m_nLastSendTicks >= KeepaliveMs * 1000)
	{
		SendCommand (CmdPing);
	}
}

boolean CMyKontrolHost::GetEvent (TMyKontrolEvent *pEvent)
{
	if (m_nEventOut == m_nEventIn)
	{
		return FALSE;
	}

	*pEvent = m_Events[m_nEventOut];
	m_nEventOut = (m_nEventOut + 1) & (EventQueueSize - 1);

	return TRUE;
}

void CMyKontrolHost::HandleSysEx (const u8 *pData, unsigned nLength)
{
	if (   nLength < 4
	    || memcmp (pData, Header, sizeof Header) != 0)
	{
		return;
	}

	u8 ucCommand = pData[3];
	pData += 4;
	nLength -= 4;

	switch (ucCommand)
	{
	case CmdHi:
		// a new session, or the keyboard lost us: handshake (again)
		if (m_bConnected)
		{
			m_bConnected = FALSE;
			PostEvent (MKEventDisconnected, 0, 0);
		}
		SendHello ();
		break;

	case CmdIdent:
		if (nLength >= 9)
		{
			m_nMode = pData[8];
			CLogger::Get ()->Write (From, LogNotice, "Connected: board %u, firmware %u.%u.%u, %u keys",
						pData[1], pData[2], pData[3], pData[4], pData[5]);
		}
		m_bConnected = TRUE;
		PostEvent (MKEventConnected, 0, 0);
		break;

	case CmdView:
		if (nLength >= 1)
		{
			m_nMode = pData[0];
			PostEvent (MKEventView, 0, m_nMode);
		}
		break;

	case CmdRefresh:
		PostEvent (MKEventRefresh, 0, 0);
		break;

	default:
		break;
	}
}

void CMyKontrolHost::HandleChannelMessage (const u8 *pMessage)
{
	if ((pMessage[0] & 0x0F) != ControlChannel)
	{
		return;
	}

	u8 ucStatus = pMessage[0] & 0xF0;
	u8 ucData1 = pMessage[1] & 0x7F;
	u8 ucData2 = pMessage[2] & 0x7F;

	if (ucStatus == 0x90 || ucStatus == 0x80)
	{
		int nOn = ucStatus == 0x90 && ucData2 != 0;
		if (ucData1 < 0x40)
		{
			if (ucData1 == ControlShift)
			{
				m_bShift = nOn;
			}
			PostEvent (MKEventButton, ucData1, nOn);
		}
		else if (ucData1 < 0x48)
		{
			PostEvent (MKEventKnobTouch, ucData1 - 0x40, nOn);
		}
		else if (ucData1 < 0x4A)
		{
			PostEvent (MKEventStripTouch, ucData1 - 0x48, nOn);
		}
	}
	else if (ucStatus == 0xB0)
	{
		// relative, signed bit: 1..63 = +n, 65..127 = -(v - 64)
		int nSteps = ucData2 < 64 ? ucData2 : -(ucData2 - 64);
		if (ucData1 >= 0x10 && ucData1 < 0x18)
		{
			PostEvent (MKEventKnob, ucData1 - 0x10, nSteps);
		}
		else if (ucData1 == 0x30)
		{
			PostEvent (MKEventEncoder, 0, nSteps);
		}
	}
}

void CMyKontrolHost::PostEvent (TMyKontrolEventType Type, unsigned nControl, int nValue)
{
	unsigned nNext = (m_nEventIn + 1) & (EventQueueSize - 1);
	if (nNext == m_nEventOut)
	{
		return;
	}

	m_Events[m_nEventIn].Type = Type;
	m_Events[m_nEventIn].nControl = nControl;
	m_Events[m_nEventIn].nValue = nValue;
	m_nEventIn = nNext;
}

void CMyKontrolHost::SendHello (void)
{
	u8 Data[3 + 2*25];
	unsigned n = 0;
	Data[n++] = 2;					// protocol version
	Data[n++] = (m_nFeatures >> 7) & 0x7F;
	Data[n++] = m_nFeatures & 0x7F;
	n += PutString (Data + n, m_pName, 24);
	n += PutString (Data + n, m_pVersion, 16);
	SendCommand (CmdHello, Data, n);

	m_bHelloPending = FALSE;
	m_nLastHelloTicks = CTimer::Get ()->GetClockTicks ();
}

void CMyKontrolHost::SendModes (unsigned nCount, const u8 *pIDs, const u8 *pFlags,
				const char *const *ppNames)
{
	u8 Data[1 + 8 * 19];
	assert (nCount <= 8);
	unsigned n = 0;
	Data[n++] = nCount;
	for (unsigned i = 0; i < nCount; i++)
	{
		Data[n++] = pIDs[i];
		Data[n++] = pFlags[i];
		n += PutString (Data + n, ppNames[i], 16);
	}
	SendCommand (CmdModes, Data, n);
}

void CMyKontrolHost::SetMode (unsigned nMode)
{
	u8 ucMode = nMode;
	SendCommand (CmdSetMode, &ucMode, 1);
}

void CMyKontrolHost::SendPage (unsigned nMode, const char *pTitle, const char *pSubtitle,
			       unsigned nBank, unsigned nBanks)
{
	m_ucGen = m_ucGen >= 127 ? 1 : m_ucGen + 1;

	u8 Data[4 + 2*25];
	unsigned n = 0;
	Data[n++] = m_ucGen;
	Data[n++] = nMode;
	Data[n++] = nBank;
	Data[n++] = nBanks;
	n += PutString (Data + n, pTitle, 24);
	n += PutString (Data + n, pSubtitle, 24);
	SendCommand (CmdPage, Data, n);
}

void CMyKontrolHost::SendSlot (unsigned nKnob, TSlotKind Kind, const char *pName,
			       boolean bBipolar, unsigned nSteps, const char *pGroup)
{
	u8 Data[10 + 25 + 17];
	unsigned n = 0;
	Data[n++] = m_ucGen;
	Data[n++] = nKnob;
	Data[n++] = Kind;
	Data[n++] = bBipolar ? 1 : 0;
	Data[n++] = nSteps;
	Data[n++] = 0;					// default (none)
	Data[n++] = 0;
	Data[n++] = 0;					// colour: none
	Data[n++] = 0;
	Data[n++] = 0;
	n += PutString (Data + n, pName, 24);
	n += PutString (Data + n, pGroup, 16);
	SendCommand (CmdSlot, Data, n);
}

void CMyKontrolHost::SendValue (unsigned nKnob, unsigned nPos, const char *pText)
{
	if (nPos > 16383)
	{
		nPos = 16383;
	}

	u8 Data[4 + 25];
	unsigned n = 0;
	Data[n++] = m_ucGen;
	Data[n++] = nKnob;
	Data[n++] = nPos >> 7;
	Data[n++] = nPos & 0x7F;
	n += PutString (Data + n, pText, 24);
	SendCommand (CmdValue, Data, n);
}

void CMyKontrolHost::SendLight (unsigned nControl, TLightState State)
{
	const u8 Data[] = {(u8) nControl, (u8) State};
	SendCommand (CmdLight, Data, sizeof Data);
}

void CMyKontrolHost::SendTransport (boolean bPlaying, boolean bRecording, unsigned nTempo10,
				    unsigned nBeat, unsigned nBeatsPerBar)
{
	const u8 Data[] = {(u8) ((bPlaying ? 1 : 0) | (bRecording ? 2 : 0)),
			   (u8) ((nTempo10 >> 7) & 0x7F), (u8) (nTempo10 & 0x7F),
			   (u8) nBeat, (u8) nBeatsPerBar, 0};
	SendCommand (CmdTransport, Data, sizeof Data);
}

void CMyKontrolHost::SendNotify (const char *pTitle, const char *pValue)
{
	u8 Data[2*24 + 1];
	unsigned n = 0;
	for (unsigned i = 0; pTitle[i] && i < 24; i++)
	{
		Data[n++] = pTitle[i] >= 0x20 && pTitle[i] < 0x7F ? pTitle[i] : '?';
	}
	Data[n++] = 0;
	for (unsigned i = 0; pValue[i] && i < 24; i++)
	{
		Data[n++] = pValue[i] >= 0x20 && pValue[i] < 0x7F ? pValue[i] : '?';
	}
	SendCommand (CmdNotify, Data, n);
}

void CMyKontrolHost::SendText (unsigned nField, const char *pText)
{
	u8 Data[1 + 24];
	unsigned n = 0;
	Data[n++] = nField;
	for (unsigned i = 0; pText[i] && i < 24; i++)
	{
		Data[n++] = pText[i] >= 0x20 && pText[i] < 0x7F ? pText[i] : '?';
	}
	SendCommand (CmdText, Data, n);
}

// F0 7D 4D 4B cmd data F7 as USB MIDI event packets on cable 1
void CMyKontrolHost::SendCommand (u8 ucCommand, const u8 *pData, unsigned nLength)
{
	if (m_pSendHandler == 0)
	{
		return;
	}

	u8 Message[256];
	unsigned nMessage = 0;
	Message[nMessage++] = 0xF0;
	memcpy (Message + nMessage, Header, sizeof Header);
	nMessage += sizeof Header;
	Message[nMessage++] = ucCommand;
	for (unsigned i = 0; i < nLength && nMessage < sizeof Message - 1; i++)
	{
		Message[nMessage++] = pData[i] & 0x7F;
	}
	Message[nMessage++] = 0xF7;

	u8 Packets[(sizeof Message + 2) / 3 * 4];
	unsigned nPackets = 0;
	for (unsigned i = 0; i < nMessage; i += 3)
	{
		unsigned nLeft = nMessage - i;
		u8 ucCIN = nLeft > 3 ? 0x4 : 0x4 + nLeft;	// 5, 6, 7: end with 1, 2, 3 bytes
		u8 *p = Packets + nPackets;
		p[0] = (Cable << 4) | ucCIN;
		p[1] = Message[i];
		p[2] = nLeft > 1 ? Message[i + 1] : 0;
		p[3] = nLeft > 2 ? Message[i + 2] : 0;
		nPackets += 4;
	}

	(*m_pSendHandler) (Packets, nPackets, m_pSendParam);
	m_nLastSendTicks = CTimer::Get ()->GetClockTicks ();
}

// length byte, then ASCII (other characters as '?')
unsigned CMyKontrolHost::PutString (u8 *pBuffer, const char *pString, unsigned nMax)
{
	unsigned n = 0;
	for (; pString[n] && n < nMax; n++)
	{
		char c = pString[n];
		pBuffer[1 + n] = c >= 0x20 && c < 0x7F ? c : '?';
	}
	pBuffer[0] = n;

	return 1 + n;
}
