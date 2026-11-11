#pragma once

// PARTIAL_SCREEN_UPDATE - Set to 1 to enable the optimization for redrawing only the modified portion of the screen.
//                         Greatly increses the framerate.

// USE_NEW_ENCODER - Set to 1 to use the full NewEncoder library. Otherise a trimmed-down version of the code is used
//                   to save memory and reduce code dependencies. Requires modification to the NewEncoder library to support ATmega4808

// USE_WATCHDOG - Set to 1 to enable the WDT crash detection

#if defined(__AVR_ATmega4809__) // Arduino Nano Every with ATmega4809

#define PARTIAL_SCREEN_UPDATE 1
#define USE_NEW_ENCODER 1
#define USE_WATCHDOG 1
#define JOYSTICK_BITS 10

#elif defined(__AVR_ATmega4808__) // Arduino Nano Every clone with ATmega4808

#define PARTIAL_SCREEN_UPDATE 1
#define USE_NEW_ENCODER 0 // NewEncoder doesn't recognize ATmega4808 out of the box. You need to modify interrupt_pins.h to get it to compile
#define USE_WATCHDOG 1
#define JOYSTICK_BITS 10

#elif defined(ARDUINO_NANO_R4)

#define PARTIAL_SCREEN_UPDATE 1
#define USE_NEW_ENCODER 0 // NewEncoder doesn't recognize Nano R4 out of the box. The pins definitions are different
#define USE_WATCHDOG 1
#define JOYSTICK_BITS 14

#elif defined(_WIN32) // Pendant emulator

#define USE_WATCHDOG 0
#define EMULATOR
#define PARTIAL_SCREEN_UPDATE 1
#define JOYSTICK_BITS 14

#else // Add support for more hardware here
#error "Unknown microcontroller"
#endif
