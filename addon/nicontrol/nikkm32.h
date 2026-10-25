//
// nikkm32.h
//
// Native Instruments Komplete Kontrol M32, stock firmware ("MIDI Mode"):
//  - HID input report 1: buttons, knob touch and the joycoder
//  - HID output report 0x80: 21 button LEDs, 0xE0: 128 x 32 OLED
//  - over USB MIDI (not handled here): keys, pitch strip (pitch bend), mod
//    strip (CC 1), knobs 1..8 = CC 14..21 on channel 1, absolute 0..127; the
//    firmware takes a knob's value from a received CC on its number, so
//    sending CC 64 back after each change makes the knobs endless
//  - the OLED shows host images only while the host reads the MIDI port
//    (Circle's USB MIDI driver does), otherwise "Template 1 / MIDI Mode"
// Reverse engineered with port/tools/m32probe.py (MicroDexed-touch port).
//
#ifndef _nicontrol_nikkm32_h
#define _nicontrol_nikkm32_h

#include <nicontrol/nicontroller.h>
#include <circle/types.h>

class CNIKompleteKontrolM32 : public CNIControllerDevice
{
public:
	// buttons (NIEventButton), = bit in input report 1
	enum TButton
	{
		ButtonShift,
		ButtonScale,
		ButtonArp,
		ButtonUndo,
		ButtonQuantize,
		ButtonIdeas,
		ButtonLoop,
		ButtonMetro,
		ButtonTempo,
		ButtonPlay,
		ButtonRec,
		ButtonStop,
		ButtonPresetUp,
		ButtonPresetDown,
		ButtonLeft,		// "M" with shift
		ButtonRight,		// "S" with shift
		ButtonBrowser,
		ButtonPlugin,		// "MIDI" with shift
		ButtonTrack,		// "Instance" with shift
		ButtonOctaveDown,
		ButtonOctaveUp,
		ButtonJoyUp,
		ButtonJoyLeft,
		ButtonJoyRight,
		ButtonJoyDown,
		ButtonKnob1Touch,	// .. ButtonKnob1Touch + 7
		ButtonJoyClick = ButtonKnob1Touch + 8,
		ButtonUnknown	// bits 34..39
	};

	// encoders (NIEventEncoder)
	enum TEncoder
	{
		EncoderJoy		// the joycoder, positive = clockwise
	};

	// LEDs (SetLED()), panel order
	enum TLED
	{
		LEDShift,
		LEDScale,
		LEDArp,
		LEDUndo,
		LEDQuantize,
		LEDIdeas,
		LEDLoop,
		LEDMetro,
		LEDTempo,
		LEDPresetUp,
		LEDPresetDown,
		LEDPlay,
		LEDRec,
		LEDStop,
		LEDLeft,
		LEDRight,
		LEDTrack,
		LEDPlugin,
		LEDBrowser,
		LEDOctaveDown,
		LEDOctaveUp,
		LEDCount
	};

	static const unsigned KnobCC = 14;		// CC of knob 1 (.. 21)
	static const unsigned KnobCount = 8;

public:
	CNIKompleteKontrolM32 (CUSBFunction *pFunction);

	TModel GetModel (void) const override		{ return ModelKompleteKontrolM32; }
	const char *GetModelName (void) const override	{ return "Komplete Kontrol M32"; }

private:
	void DecodeReport (const u8 *pReport, unsigned nLength) override;
	boolean SendLEDs (const u8 *pBrightness) override;
	boolean SendDisplay (const u8 *pPages) override;

private:
	static const unsigned ButtonBytes = 5;
	u8 m_Buttons[ButtonBytes];
	int m_nJoyPosition;			// -1: unknown
};

#endif
