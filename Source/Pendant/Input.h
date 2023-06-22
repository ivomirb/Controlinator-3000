#pragma once

const uint8_t BUTTON_JOYSTICK = 8;
const uint8_t BUTTON_ABORT = 9;

const int JOYSTICK_STEPS = 10; // number of steps in the joystick range

#ifndef BUTTON_COUNT
#define BUTTON_COUNT 10
#endif

const uint16_t BUTTON_HOLD_TIME = 1000; // hold for 1 second

///////////////////////////////////////////////////////////////////////////////
// Buttons

uint16_t g_ButtonState; // 0 - up, 1 - down
uint16_t g_ButtonClick; // buttons clicked this frame
uint16_t g_ButtonUnclick; // buttons released this frame
uint16_t g_ButtonHold; // buttons held this frame
uint16_t g_ButtonDown; // buttons that are logically pressed
uint16_t g_ButtonChangeTimers[BUTTON_COUNT];

bool TestBit( uint16_t flags, uint8_t bit )
{
	return (flags & (1 << bit)) != 0;
}

void UpdateButtonState( uint16_t physicalState, uint16_t dt )
{
	// debounce - ignore state change within 10 ms since the last one
	uint16_t changed = g_ButtonState ^ physicalState;
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		uint16_t mask = 1 << i;
		if ((changed & mask) && g_ButtonChangeTimers[i] < 10)
		{
			physicalState ^= mask;
		}
	}

	changed = g_ButtonState ^ physicalState;
	g_ButtonClick = physicalState & ~g_ButtonState;
	g_ButtonUnclick = ~physicalState & g_ButtonState;
	g_ButtonState = physicalState;
	g_ButtonHold = 0;
	g_ButtonDown = 0;

	// update timers
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		uint16_t mask = 1 << i;
		uint16_t &timer = g_ButtonChangeTimers[i];
		if (changed & mask)
		{
			timer = 0;
		}
		else if (timer != 0xFFFF)
		{
			uint16_t t = timer + dt;
			if (t > 30000) t = 30000;
			if ((g_ButtonState & mask) && timer <= BUTTON_HOLD_TIME && t > BUTTON_HOLD_TIME)
			{
				g_ButtonHold |= mask;
				t = 0xFFFF;
			}
			timer = t;
		}

		if ((g_ButtonState & mask) && ((g_ButtonHold & mask) || timer != 0xFFFF))
		{
			g_ButtonDown |= mask;
		}
	}
}

void ReleaseAllButtons( void )
{
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		if (TestBit(g_ButtonState, i))
		{
			g_ButtonChangeTimers[i] = 0xFFFF; // the button will be considered off until it is pressed again
		}
	}
	g_ButtonDown = 0;
}

///////////////////////////////////////////////////////////////////////////////
// Joystick

// raw joystick position
uint16_t g_JoyX = 512;
uint16_t g_JoyY = 512;

void UpdateJoystick()
{
#ifndef EMULATOR
	g_JoyX = 1023 - analogRead(g_JoyPinX);
	g_JoyY = 1023 - analogRead(g_JoyPinY);
#endif
}

// Quantizes the raw joystick position to range [-JOYSTICK_STEPS..JOYSTICK_STEPS] based on the calibration settings
int8_t QuantizeJoystick( uint16_t val, const uint16_t calibration[4] )
{
	if (val < calibration[1])
	{
		int8_t res = - 1 - ((calibration[1] - val - 1) * JOYSTICK_STEPS) / (calibration[1] - calibration[0]);
		return res > -JOYSTICK_STEPS ? res : -JOYSTICK_STEPS;
	}

	if (val > calibration[2])
	{
		int8_t res = 1 + (val - calibration[2] - 1) * JOYSTICK_STEPS / (calibration[3] - calibration[2]);
		return res < JOYSTICK_STEPS ? res : JOYSTICK_STEPS;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Wheel
// Reading of hand wheel, greatly simplified version of https://github.com/gfvalvo/NewEncoder
// Converted to hard-coded values and global variables to save RAM

const int16_t ENCODER_MIN_VALUE = -32000;
const int16_t ENCODER_MAX_VALUE = 32000;

#ifdef EMULATOR

int16_t g_EncoderValue;

void InitializeEncoder( void )
{
}

int16_t EncoderDrainValue( void )
{
	int16_t val = g_EncoderValue;
	g_EncoderValue = 0;
	return val;
}

void EncoderAddValue( int8_t add )
{
	g_EncoderValue += add;
}

#elif USE_NEW_ENCODER

#include "NewEncoder.h"

class HandWheelEncoder : public NewEncoder
{
public:
	HandWheelEncoder( void ):
		NewEncoder(g_EncoderPinA, g_EncoderPinB, ENCODER_MIN_VALUE, ENCODER_MAX_VALUE, 0, HALF_PULSE)
	{
	}

	int16_t DrainValue( void )
	{
		NewEncoder::EncoderState currentEncoderState;
		if (getState(currentEncoderState))
		{
			int16_t delta = currentEncoderState.currentValue / 2;
			if (delta != 0)
			{
				noInterrupts();
				liveState.currentValue -= delta * 2;
				interrupts();
				return delta;
			}
		}
		return 0;
	}
};

HandWheelEncoder g_HandWheelEncoder;

void InitializeEncoder( void )
{
	g_HandWheelEncoder.begin();
	NewEncoder::EncoderState state;
	g_HandWheelEncoder.getState(state);
}

int16_t EncoderDrainValue( void )
{
	return g_HandWheelEncoder.DrainValue();
}

#else

#define NE_STATE_MASK 0b00000111
#define NE_DELTA_MASK 0b00011000
#define NE_INCREMENT_DELTA 0b00001000
#define NE_DECREMENT_DELTA 0b00010000

// Define states and transition table for "one pulse per two detents" type encoder
#define NE_DETENT_0 0b000
#define NE_DETENT_1 0b111
#define NE_DEBOUNCE_0 0b010
#define NE_DEBOUNCE_1 0b001
#define NE_DEBOUNCE_2 0b101
#define NE_DEBOUNCE_3 0b110

const uint8_t g_NE_halfPulseTransitionTable[][4] PROGMEM =
{
	{ NE_DETENT_0, NE_DEBOUNCE_1, NE_DETENT_0, NE_DEBOUNCE_0 },  // DETENT_0 0b000
	{ NE_DETENT_0, NE_DEBOUNCE_1, NE_DEBOUNCE_1, NE_DETENT_1 | NE_INCREMENT_DELTA }, // DEBOUNCE_1 0b001
	{ NE_DEBOUNCE_0, NE_DETENT_1 | NE_DECREMENT_DELTA, NE_DETENT_0, NE_DEBOUNCE_0 },  // DEBOUNCE_0 0b010
	{ NE_DETENT_1, NE_DETENT_1, NE_DETENT_1, NE_DETENT_1 },  // 0b011 - illegal state should never be in it
	{ NE_DETENT_0, NE_DETENT_0, NE_DETENT_0, NE_DETENT_0 },  // 0b100 - illegal state should never be in it
	{ NE_DETENT_0 | NE_DECREMENT_DELTA, NE_DEBOUNCE_2, NE_DEBOUNCE_2, NE_DETENT_1 }, // DEBOUNCE_2 0b101
	{ NE_DEBOUNCE_3, NE_DETENT_1, NE_DETENT_0 | NE_INCREMENT_DELTA, NE_DEBOUNCE_3 },  // DEBOUNCE_3 0b110
	{ NE_DEBOUNCE_3, NE_DETENT_1, NE_DEBOUNCE_2, NE_DETENT_1 }  // DETENT_1 0b111
};

uint8_t *g_aPin_register, *g_bPin_register;
uint8_t g_aPin_bitmask, g_bPin_bitmask;

enum EncoderClick {
	NoClick, DownClick, UpClick
};

struct EncoderState
{
	int16_t currentValue = 0;
	int8_t currentClick = NoClick;
};

volatile EncoderState g_EncoderLiveState;
volatile bool g_EncoderStateChanged;
EncoderState g_EncoderLocalState;
volatile uint8_t g_EncoderPinAValue, g_EncoderPinBValue;
volatile uint8_t g_CurrentEncoderState;

bool GetEncoderState(EncoderState &state)
{
	bool localStateChanged = g_EncoderStateChanged;
	if (localStateChanged)
	{
		noInterrupts();
		memcpy(&g_EncoderLocalState, &g_EncoderLiveState, sizeof(EncoderState));
		g_EncoderStateChanged = false;
		interrupts();
	}
	else
	{
		g_EncoderLocalState.currentClick = NoClick;
	}
	state = g_EncoderLocalState;
	return localStateChanged;
}

void EncoderPinChangeHandler(uint8_t index)
{
	uint8_t newStateVariable = pgm_read_byte(&g_NE_halfPulseTransitionTable[g_CurrentEncoderState][index]);
	g_CurrentEncoderState = newStateVariable & NE_STATE_MASK;
	if ((newStateVariable & NE_DELTA_MASK) != 0)
	{
		if ((newStateVariable & NE_DELTA_MASK) == NE_INCREMENT_DELTA)
		{
			g_EncoderLiveState.currentClick = UpClick;
			if (g_EncoderLiveState.currentValue < ENCODER_MAX_VALUE)
			{
				g_EncoderLiveState.currentValue++;
			}
		}
		else if ((newStateVariable & NE_DELTA_MASK) == NE_DECREMENT_DELTA)
		{
			g_EncoderLiveState.currentClick = DownClick;
			if (g_EncoderLiveState.currentValue > ENCODER_MIN_VALUE)
			{
				g_EncoderLiveState.currentValue--;
			}
		}
		g_EncoderStateChanged = true;
	}
}

void EncoderPinAChange( void )
{
	uint8_t newPinValue = ((*g_aPin_register) & g_aPin_bitmask) ? 1 : 0;
	if (newPinValue != g_EncoderPinAValue)
	{
		g_EncoderPinAValue = newPinValue;
		EncoderPinChangeHandler(0b00 | g_EncoderPinAValue);  // Falling aPin == 0b00, Rising aPin = 0b01;
	}
}

void EncoderPinBChange( void )
{
	uint8_t newPinValue = ((*g_bPin_register) & g_bPin_bitmask) ? 1 : 0;
	if (newPinValue != g_EncoderPinBValue)
	{
		g_EncoderPinBValue = newPinValue;
		EncoderPinChangeHandler(0b10 | g_EncoderPinBValue);  // Falling bPin == 0b10, Rising bPin = 0b11;
	}
}

void InitializeEncoder( void )
{
	pinMode(g_EncoderPinA, INPUT_PULLUP);
	pinMode(g_EncoderPinB, INPUT_PULLUP);
	g_aPin_register = portInputRegister(digitalPinToPort(g_EncoderPinA));
	g_bPin_register = portInputRegister(digitalPinToPort(g_EncoderPinB));
	g_aPin_bitmask = digitalPinToBitMask(g_EncoderPinA);
	g_bPin_bitmask = digitalPinToBitMask(g_EncoderPinB);

	delay(2); // Seems to help ensure first reading after pinMode is correct
	g_EncoderPinAValue = ((*g_aPin_register) & g_aPin_bitmask) ? 1 : 0;
	g_EncoderPinBValue = ((*g_bPin_register) & g_bPin_bitmask) ? 1 : 0;

	g_CurrentEncoderState = (g_EncoderPinBValue << 1) | g_EncoderPinAValue;

	if (g_CurrentEncoderState == (NE_DETENT_1 & 0b11))
	{
		g_CurrentEncoderState = NE_DETENT_1;
	}

	attachInterrupt(digitalPinToInterrupt(g_EncoderPinA), EncoderPinAChange, CHANGE);
	attachInterrupt(digitalPinToInterrupt(g_EncoderPinB), EncoderPinBChange, CHANGE);
}

int16_t EncoderDrainValue( void )
{
	EncoderState currentEncoderState;
	if (GetEncoderState(currentEncoderState))
	{
		int16_t delta = currentEncoderState.currentValue / 2;
		if (delta != 0)
		{
			noInterrupts();
			g_EncoderLiveState.currentValue -= delta * 2;
			interrupts();
			return delta;
		}
	}
	return 0;
}

#endif
