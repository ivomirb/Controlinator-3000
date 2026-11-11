#pragma once

enum CrashReason
{
	CRASH_NONE,
	CRASH_WATCHDOG,
	CRASH_BROWNOUT,
};

CrashReason InitializeWatchdog( void )
{
	CrashReason reason = CRASH_NONE;

#if defined(__AVR_ATmega4808__) || defined(__AVR_ATmega4809__)

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

	if (R_SYSTEM->RSTSR0 & R_SYSTEM_RSTSR0_LVD0RF_Msk)
	{
		reason = CRASH_BROWNOUT;
	}
	else if (R_SYSTEM->RSTSR1 & R_SYSTEM_RSTSR1_SWRF_Msk)
	{
		reason = CRASH_NONE;
	}
	else if (R_SYSTEM->RSTSR1 & R_SYSTEM_RSTSR1_WDTRF_Msk)
	{
		reason = CRASH_WATCHDOG;
	}
	R_SYSTEM->RSTSR1 = 0;

	WDT.begin(1000);

#endif

	return reason;
}

void TickWatchdog( void )
{
#if defined(__AVR_ATmega4808__) || defined(__AVR_ATmega4809__)

	wdt_reset();

#elif defined(ARDUINO_NANO_R4)

	WDT.refresh();

#endif
}
