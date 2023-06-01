#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

void InitializeInput( void );
uint16_t ReadButtons( void );

#define Sprintf sprintf
#define Strlen strlen
#define Assert(x) (0)
#define ROMSTR(x) F(x)
#define DEFINE_STRING(name, str) const char PROGMEM name##Str[] = str; const __FlashStringHelper *name = (__FlashStringHelper*)name##Str;

#define U8G2_FULL_BUFFER 0

const int g_JoyPinX = A6;
const int g_JoyPinY = A7;

#include "Main.h"

#ifdef ENABLE_ABORT_BUTTON
const uint8_t g_ButtonPins[BUTTON_COUNT] PROGMEM = {A3, A2, A1, A0, 6, 7, 8, 9, 12, 5};
#else
const uint8_t g_ButtonPins[BUTTON_COUNT] PROGMEM = {A3, A2, A1, A0, 6, 7, 8, 9, 12};
#endif

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
