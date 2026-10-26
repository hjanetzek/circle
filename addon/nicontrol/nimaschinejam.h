//
// nimaschinejam.h
//
// Native Instruments Maschine Jam (no MIDI interface, all HID):
//  - HID input report 1: main encoder (byte 1, 4 bit counter) and 120 button
//    bits (bytes 2..16), including the 64 pads (no velocity)
//  - HID input report 2: 8 touch strips x (time, finger 1, finger 2), u16 LE,
//    position 0x000..0x3ff from bottom to top, 0 = not touched
//  - HID output report 0x80: 53 mono LEDs (buttons, level meters), 0..127
//  - HID output report 0x81: 80 RGB LEDs (scene buttons, pads, groups) and
//    0x82: 88 strip LEDs (8 x 11, bottom to top), palette index (Colour())
// Report layout and LED order from ni-usb/cabl (C) 2015 Vincenzo Pacella, MIT
// license; checked on the device with port/tools/niprobe.py (MicroDexed-touch
// port).
//
#ifndef _nicontrol_nimaschinejam_h
#define _nicontrol_nimaschinejam_h

#include <nicontrol/nicontroller.h>
#include <circle/types.h>

class CNIMaschineJam : public CNIControllerDevice
{
public:
	// buttons (NIEventButton), = bit in input report 1 (from byte 2)
	enum TButton
	{
		ButtonSong,
		ButtonScene1,		// .. ButtonScene1 + 7, above the pads
		ButtonStep = ButtonScene1 + 8,
		ButtonPadMode,
		ButtonClear,
		ButtonDuplicate,
		ButtonUp,
		ButtonLeft,
		ButtonRight,
		ButtonDown,
		ButtonNoteRepeat,
		ButtonPad1,		// .. ButtonPad1 + 63, row major from the top left
		ButtonGroupA = ButtonPad1 + 64,	// .. ButtonGroupA + 7, below the pads
		ButtonMst = ButtonGroupA + 8,
		ButtonGrp,
		ButtonIn,
		ButtonCue,
		ButtonBrowse,
		ButtonMacro,
		ButtonLevel,
		ButtonAux,
		ButtonControl,
		ButtonAuto,
		ButtonPerform,
		ButtonVariation,
		ButtonLock,
		ButtonTune,
		ButtonSwing,
		ButtonShift,
		ButtonPlay,
		ButtonRec,
		ButtonPageLeft,
		ButtonPageRight,
		ButtonTempo,
		ButtonGrid,
		ButtonSolo,
		ButtonMute,
		ButtonSelect,
		ButtonEncoderTouch,
		ButtonEncoderPush,
		ButtonCount
	};

	// encoders (NIEventEncoder)
	enum TEncoder
	{
		EncoderMain		// positive = clockwise
	};

	// touch strips (NIEventStrip): nControl = strip 0..7 (left to right),
	// nValue = finger 1 position 1..StripMax (bottom to top), -1 released
	static const unsigned StripCount = 8;
	static const int StripMax = 0x3FF;

	// mono LEDs (SetLED()), brightness 0..127
	enum TLED
	{
		LEDSong,
		LEDStep,
		LEDPadMode,
		LEDClear,
		LEDDuplicate,
		LEDUp,
		LEDLeft,
		LEDRight,
		LEDDown,
		LEDNoteRepeat,
		LEDMst,
		LEDGrp,
		LEDIn,
		LEDCue = 14,
		LEDBrowse = 16,
		LEDMacro,
		LEDLevel,
		LEDAux,
		LEDControl,
		LEDAuto,
		LEDPerform,
		LEDVariation,
		LEDLock,
		LEDTune,
		LEDSwing,
		LEDShift,
		LEDPlay,
		LEDRec,
		LEDPageLeft,
		LEDPageRight,
		LEDTempo,
		LEDGrid,
		LEDSolo,
		LEDMute,
		LEDSelect,
		LEDLevelLeft1,		// .. LEDLevelLeft1 + 7, bottom to top
		LEDLevelRight1 = LEDLevelLeft1 + 8,
		LEDCount = LEDLevelRight1 + 8
	};

	// palette: Colour (hue, level); level 0..3 = dim, medium, full, pale
	enum THue
	{
		HueOff,
		HueRed,
		HueOrange,
		HueLightOrange,
		HueWarmYellow,
		HueYellow,
		HueLime,
		HueGreen,
		HueMint,
		HueCyan,
		HueTurquoise,
		HueBlue,
		HuePlum,
		HueViolet,
		HuePurple,
		HueMagenta,
		HueFuchsia,
		HueWhite
	};

	static u8 Colour (THue Hue, unsigned nLevel = 2)
	{
		return Hue == HueOff ? 0 : Hue * 4 + (nLevel & 3);
	}

	static const unsigned PadCount = 64;
	static const unsigned StripLEDs = 11;

public:
	CNIMaschineJam (CUSBFunction *pFunction);

	TModel GetModel (void) const override		{ return ModelMaschineJam; }
	const char *GetModelName (void) const override	{ return "Maschine Jam"; }

	// RGB LEDs, ucColour from Colour()
	void SetPad (unsigned nPad, u8 ucColour);		// 0..63, row major from the top left
	void SetScene (unsigned nScene, u8 ucColour);		// 0..7
	void SetGroup (unsigned nGroup, u8 ucColour);		// 0..7 (A..H)
	void SetStripLED (unsigned nStrip, unsigned nLED, u8 ucColour); // nLED 0 = bottom

	// strip nStrip as a bar of nLEDs (0..11) LEDs from the bottom
	void SetStripBar (unsigned nStrip, unsigned nLEDs, u8 ucColour);

private:
	void DecodeReport (const u8 *pReport, unsigned nLength) override;
	boolean SendLEDs (const u8 *pBrightness) override;
	boolean SendDisplay (const u8 *pPages) override		{ return TRUE; }
	void UpdateModel (void) override;

	void DecodeButtons (const u8 *pReport, unsigned nLength);
	void DecodeStrips (const u8 *pReport, unsigned nLength);

	void SetRGB (unsigned nIndex, u8 ucColour);

private:
	static const unsigned ButtonBytes = 15;
	u8 m_Buttons[ButtonBytes];
	int m_nEncoder;				// -1: unknown
	int m_nStrip[StripCount];		// last position, 0: not touched

	// report 0x81: 0..7 scene buttons, 8..71 pads, 72..79 groups
	static const unsigned RGBLEDs = 80;
	u8 m_RGB[RGBLEDs];
	boolean m_bRGBChanged;

	u8 m_StripLED[StripCount * StripLEDs];
	boolean m_bStripLEDsChanged;
};

#endif
