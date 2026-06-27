#pragma once

enum CrashReason
{
	CRASH_NONE,
	CRASH_WATCHDOG,
#if !defined(__AVR_ATmega328P__)
	CRASH_BROWNOUT,
#endif
};

#if defined(__AVR_ATmega328P__)
// I was unable to reliably get the reset flags on 328P, so instead, the watchdog interrupt writes a flag to the EEPROM
ISR(WDT_vect, ISR_NAKED)
{
	g_RomSettings.bCrash = 1;
	EEPROM.put(SETTINGS_ROM_ADDRESS + offsetof(RomSettings, bCrash), g_RomSettings.bCrash);
	wdt_enable(WDTO_120MS);
	while (1) {}
}
#endif

uint16_t rstr0, rstr1;

CrashReason InitializeWatchdog( void )
{
	CrashReason reason = CRASH_NONE;

#if defined(__AVR_ATmega328P__)

	if (g_RomSettings.bCrash)
	{
		reason = CRASH_WATCHDOG;
		g_RomSettings.bCrash = false;
		EEPROM.put(SETTINGS_ROM_ADDRESS + offsetof(RomSettings, bCrash), g_RomSettings.bCrash);
	}

	cli();
	wdt_reset();
	WDTCSR |= _BV(WDCE) | _BV(WDE);
	WDTCSR = _BV(WDIE) | _BV(WDE) | WDTO_1S;
	sei();

#elif defined(__AVR_ATmega4808__) || defined(__AVR_ATmega4809__)

	if (RSTCTRL.RSTFR & RSTCTRL_WDRF_bm)
	{
		reason = CRASH_WATCHDOG;
	}
	else if (RSTCTRL.RSTFR & RSTCTRL_BORF_bm)
	{
		reason = CRASH_BROWNOUT;
	}

	wdt_reset();
	wdt_enable(WDTO_1S);
	RSTCTRL.RSTFR = 0xFF;

#elif defined(ARDUINO_NANO_R4)

	rstr0 = R_SYSTEM->RSTSR0;
	rstr1 = R_SYSTEM->RSTSR1;
	if (R_SYSTEM->RSTSR0 & R_SYSTEM_RSTSR0_LVD0RF_Msk)
	{
		reason = CRASH_BROWNOUT;
	}
	else if (R_SYSTEM->RSTSR1 & R_SYSTEM_RSTSR1_WDTRF_Msk)
	{
		reason = CRASH_WATCHDOG;
	}

	WDT.begin(1000);

#endif

	return reason;
}

void TickWatchdog( void )
{
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega4808__) || defined(__AVR_ATmega4809__)

	wdt_reset();

#elif defined(ARDUINO_NANO_R4)

	WDT.refresh();

#endif
}
