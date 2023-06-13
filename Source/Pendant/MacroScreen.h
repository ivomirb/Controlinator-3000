void MacroScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	if (!s_DrawState.bDrawAll) return;
#endif
	DrawMachineStatus();
	DrawUnusedButtons(m_UnusedMacros);
	for (uint8_t i = 0; i < 7; i++)
	{
		if (TestBit(m_UnusedMacros, i))
		{
			continue;
		}
		const char *name = m_MacroNames[i];
		DrawButton(i, name, Strlen(name), TestBit(m_MacroHoldFlags, i));
	}

	SetColorIndex(1);
	DrawText(14, 4, g_StrBack);
}

void MacroScreen::Update( unsigned long time )
{
	for (uint8_t i = 0; i < 7; i++)
	{
		if (TestBit(m_UnusedMacros, i))
		{
			continue;
		}
		if (TestBit(m_MacroHoldFlags, i))
		{
			if (!TestBit(g_ButtonHold, i))
			{
				continue;
			}
		}
		else if (!TestBit(g_ButtonClick, i))
		{
			continue;
		}

		Serial.print(ROMSTR("RUNMACRO:"));
		Serial.println(i + 1);
		return;
	}

	if (GetCurrentButton() == BUTTON_BACK)
	{
		CloseScreen();
	}
}

// Parses the MACROS: string from the PC
// <hold flags>|<macro1>|<macro2>|<macro3>|<macro4>|<macro5>|<macro6>|<macro7>|
void MacroScreen::ParseMacros( const char *macros )
{
	m_MacroHoldFlags = atol(macros);
	m_UnusedMacros = 0;

	macros = strchr(macros, '|') + 1;
	for (uint8_t idx = 0; idx < 7; idx++)
	{
		const char *end = strchr(macros, '|');
		int16_t len = (int16_t)(end - macros);
		if (len > 8) len = 8;
		char *name = m_MacroNames[idx];
		memcpy(name, macros, len);
		name[len] = 0;
		if (len == 0)
		{
			m_UnusedMacros |= 1 << idx;
		}
		macros = end + 1;
	}

#if PARTIAL_SCREEN_UPDATE
	s_DrawState.bDrawAll = true;
#endif
}
