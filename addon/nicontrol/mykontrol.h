//
// mykontrol.h
//
// Host side of the My Kontrol protocol (version 2), for keyboards running the
// open My Kontrol firmware (e.g. the Komplete Kontrol M32): handshake,
// keepalive, controls as events, and knob pages, lights, transport and
// popups for the keyboard's screen and lights.
// Protocol: https://github.com/Gori/my-kontrol/blob/main/docs/protocol.md
//
// The keyboard is a USB MIDI device with two cables: 0 "Keys" (music) and
// 1 "DAW" (this protocol). The application hands over the DAW cable's USB
// MIDI event packets (ReceivePacket(), any context) and sends what this
// class produces (send handler, task level). Process() runs at task level.
//
#ifndef _nicontrol_mykontrol_h
#define _nicontrol_mykontrol_h

#include <circle/string.h>
#include <circle/types.h>

enum TMyKontrolEventType
{
	MKEventConnected,	// handshake done: send MODES and the full state
	MKEventDisconnected,	// host lost (no answer to HELLO)
	MKEventView,		// nValue: the current mode (knob view)
	MKEventRefresh,		// the keyboard wants the full state again
	MKEventButton,		// nControl: control ID, nValue: 1 pressed, 0 released
	MKEventKnob,		// nControl: knob 0.., nValue: steps (signed)
	MKEventKnobTouch,	// nControl: knob 0.., nValue: 1 touched, 0 released
	MKEventStripTouch,	// nControl: 0 pitch, 1 mod, nValue: 1 / 0
	MKEventEncoder		// nValue: steps of the 4D encoder (signed)
};

struct TMyKontrolEvent
{
	u8 Type;		// TMyKontrolEventType
	u8 nControl;
	s16 nValue;
};

class CMyKontrolHost
{
public:
	// control IDs (buttons)
	enum TControl
	{
		ControlShift, ControlScale, ControlArp, ControlUndo, ControlQuantize,
		ControlIdeas, ControlLoop, ControlMetro, ControlTempo, ControlPlay,
		ControlRec, ControlStop, ControlPresetUp, ControlPresetDown,
		ControlMute, ControlSolo, ControlBrowser, ControlPlugin, ControlTrack,
		ControlOctaveDown, ControlOctaveUp,
		Control4DUp, Control4DLeft, Control4DRight, Control4DDown, Control4DPush,
		ControlPedal
	};

	// knob views (modes)
	enum TMode
	{
		ModePlugin = 0, ModeVolume = 1, ModeClips = 2, ModePan = 3,
		ModeSends = 4, ModeInstrument = 5, ModeHost = 16	// 16..19 host-defined
	};

	enum TLightState { LightOff, LightDim, LightOn, LightBlink };

	enum TSlotKind { SlotEmpty, SlotContinuous, SlotToggle, SlotEnum };

	// features (HELLO)
	static const unsigned FeatureParameters = 1 << 0;
	static const unsigned Feature4D = 1 << 8;

	// USB MIDI event packets (cable 1) to the keyboard
	typedef boolean TSendHandler (const u8 *pPackets, unsigned nLength, void *pParam);

public:
	CMyKontrolHost (const char *pName, const char *pVersion,
			unsigned nFeatures = FeatureParameters | Feature4D);

	void SetSendHandler (TSendHandler *pHandler, void *pParam);

	// a USB MIDI event packet of the DAW cable (any context, e.g. USB IRQ)
	void ReceivePacket (const u8 *pPacket);

	// handshake, keepalive, parsing; call often (task level)
	void Process (void);

	// next control event, FALSE if none (after Process())
	boolean GetEvent (TMyKontrolEvent *pEvent);

	boolean IsConnected (void) const	{ return m_bConnected; }
	unsigned GetMode (void) const		{ return m_nMode; }
	boolean IsShift (void) const		{ return m_bShift; }

	// a device was attached or detached: start / forget the session
	void Reset (void);

	// state for the keyboard (only while connected)
	void SendModes (unsigned nCount, const u8 *pIDs, const u8 *pFlags, const char *const *ppNames);
	void SetMode (unsigned nMode);
	void SendPage (unsigned nMode, const char *pTitle, const char *pSubtitle,
		       unsigned nBank = 0, unsigned nBanks = 0);		// clears the slots
	void SendSlot (unsigned nKnob, TSlotKind Kind, const char *pName,
		       boolean bBipolar = FALSE, unsigned nSteps = 0, const char *pGroup = "");
	void SendValue (unsigned nKnob, unsigned nPos, const char *pText);	// nPos 0..16383
	void SendLight (unsigned nControl, TLightState State);
	void SendTransport (boolean bPlaying, boolean bRecording, unsigned nTempo10,
			    unsigned nBeat = 127, unsigned nBeatsPerBar = 4);
	void SendNotify (const char *pTitle, const char *pValue);
	void SendText (unsigned nField, const char *pText);

private:
	void HandleSysEx (const u8 *pData, unsigned nLength);	// without F0 / F7
	void HandleChannelMessage (const u8 *pMessage);
	void PostEvent (TMyKontrolEventType Type, unsigned nControl, int nValue);

	void SendHello (void);
	void SendCommand (u8 ucCommand, const u8 *pData = 0, unsigned nLength = 0);
	static unsigned PutString (u8 *pBuffer, const char *pString, unsigned nMax);

private:
	const char *m_pName;
	const char *m_pVersion;
	unsigned m_nFeatures;

	TSendHandler *m_pSendHandler;
	void *m_pSendParam;

	// received packets (ReceivePacket() -> Process())
	static const unsigned RxQueueSize = 256;	// power of 2
	u8 m_RxQueue[RxQueueSize][4];
	volatile unsigned m_nRxIn;
	volatile unsigned m_nRxOut;

	static const unsigned MaxSysEx = 1024;
	u8 m_SysEx[MaxSysEx];
	unsigned m_nSysExLength;
	boolean m_bInSysEx;

	static const unsigned EventQueueSize = 64;	// power of 2
	TMyKontrolEvent m_Events[EventQueueSize];
	unsigned m_nEventIn;
	unsigned m_nEventOut;

	boolean m_bConnected;
	boolean m_bHelloPending;
	unsigned m_nMode;
	boolean m_bShift;
	u8 m_ucGen;			// of the current page
	unsigned m_nLastSendTicks;
	unsigned m_nLastHelloTicks;
};

#endif
