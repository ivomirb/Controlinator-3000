#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

void InitializeInput( void );
uint16_t ReadButtons( void );

#define Sprintf sprintf
#define Strlen strlen
#define Strcpy strcpy
#define Assert(x) (0)
#define ROMSTR(x) F(x)
#define DEFINE_STRING(name, str) const char PROGMEM name##Str[] = str; const __FlashStringHelper *name = (__FlashStringHelper*)name##Str;

const int g_EncoderPinA = 2;
const int g_EncoderPinB = 3;
const int g_JoyPinX = A6; // X and Y are swapped to match how the joystick is mounted on the pendant
const int g_JoyPinY = A7;
const uint8_t g_ButtonPins[] PROGMEM = {A3, A2, A1, A0, 7, 8, 9, 10, 12, 6};

#include "Main.h"

static_assert(sizeof(g_ButtonPins) == BUTTON_COUNT, "wrong button count");

void InitializeInput( void )
{
  for (uint16_t i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(pgm_read_byte(&g_ButtonPins[i]), INPUT_PULLUP);
  }
	pinMode(g_JoyPinX, INPUT);
	pinMode(g_JoyPinY, INPUT);
}

uint16_t ReadButtons( void )
{
  uint16_t buttons = 0;
  for (uint16_t i = 0; i < BUTTON_COUNT; i++)
  {
    if (digitalRead(pgm_read_byte(&g_ButtonPins[i])) == LOW) buttons |= 1 << i;
  }
  return buttons;
}
