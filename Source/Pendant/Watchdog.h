#pragma once

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

bool InitializeWatchdog( void )
{
	bool bCrash = false;

#if defined(__AVR_ATmega328P__)

	if (g_RomSettings.bCrash)
	{
		bCrash = true;
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
		bCrash = true;
	}

	wdt_reset();
	wdt_enable(WDTO_1S);
	RSTCTRL.RSTFR = 0xFF;

#endif

	return bCrash;
}

void TickWatchdog( void )
{
	wdt_reset();
}
