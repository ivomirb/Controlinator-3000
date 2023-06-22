#pragma once

// U8G2_FULL_BUFFER - Set to 1 to keep a copy of the screen buffer. Requires more RAM

// PARTIAL_SCREEN_UPDATE - Set to 1 to enable the optimization for redrawing only the modified portion of the screen.
//                         Greatly increses the framerate. Requires U8G2_FULL_BUFFER

// USE_NEW_ENCODER - Set to 1 to use the full NewEncoder library. Otherise a trimmed-down version of the code is used
//                   to save memory and reduce code dependencies

#if defined(__AVR_ATmega328P__) // Arduino Nano with ATmega328P

#define U8G2_FULL_BUFFER 0
#define PARTIAL_SCREEN_UPDATE 0
#define USE_NEW_ENCODER 0 // You can set to 1 (for example to test a new wheel hardware), but it will disable some other features to save memory

#if USE_NEW_ENCODER
// Disable few of the non-essential screens to free up some memory for the NewEncoder library
// To save even more memory, disable the macro U8G2_16BIT in U8g2\src\clib\u8g2.h
#define DISABLE_WELCOME_SCREEN
#define DISABLE_MACRO_SCREEN
#define DISABLE_CALIBRATION_SCREEN
#endif

#elif defined(__AVR_ATmega4809__) // Arduino Nano Every with ATmega4809

#define U8G2_FULL_BUFFER 1
#define PARTIAL_SCREEN_UPDATE 1
#define USE_NEW_ENCODER 1

#elif defined(__AVR_ATmega4808__) // Arduino Nano Every clone with ATmega4808

#define U8G2_FULL_BUFFER 1
#define PARTIAL_SCREEN_UPDATE 1
#define USE_NEW_ENCODER 0 // NewEncoder doesn't recognize ATmega4808 out of the box. You need to modify interrupt_pins.h to get it to compile

#elif defined(_WIN32) // Pendant emulator

#define EMULATOR
#define U8G2_FULL_BUFFER 1
#define PARTIAL_SCREEN_UPDATE 1

#else // Add support for more harware here
#error "Unknown microcontroller"
#endif

#if PARTIAL_SCREEN_UPDATE && !U8G2_FULL_BUFFER
#error PARTIAL_SCREEN_UPDATE requires U8G2_FULL_BUFFER
#endif
