void ProbeMenuScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->tloState != g_TloState;
	pDrawState->tloState = g_TloState;
	if (!bDrawAll) return;
	if (!s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}
#endif

	DrawMachineStatus();
	uint8_t unusedButtons = 0x78;
	DrawButton(BUTTON_PROBE_Z, ROMSTR("Probe Z"), 7, false);
	DrawButton(BUTTON_PROBE_REF_TOOL, ROMSTR("Probe Ref Tool"), 14, false);
	if (g_TloState & TLO_HAS_REF)
	{
		DrawButton(BUTTON_PROBE_NEW_TOOL, ROMSTR("Probe New Tool"), 14, false);
	}
	else
	{
		unusedButtons |= 1 << BUTTON_PROBE_NEW_TOOL;
	}
	DrawButton(BUTTON_BACK, g_StrBack, 4, false);
	DrawUnusedButtons(unusedButtons);
}

void ProbeMenuScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (button == BUTTON_PROBE_Z)
	{
		g_ZProbeScreen.Activate(time, ZProbeScreen::PROBE_Z, true);
	}
	else if (button == BUTTON_PROBE_REF_TOOL)
	{
		if (g_bRecentlyHomed)
		{
			g_ZProbeScreen.Activate(time, ZProbeScreen::PROBE_REF_TOOL, true);
		}
		else
		{
			Serial.print(g_StrPROBE);
			Serial.print(g_StrENTER);
			Serial.println(ZProbeScreen::PROBE_REF_TOOL);
		}
	}
	else if ((g_TloState & TLO_HAS_REF) && button == BUTTON_PROBE_NEW_TOOL)
	{
		if (g_bRecentlyHomed)
		{
			g_ZProbeScreen.Activate(time, ZProbeScreen::PROBE_NEW_TOOL, true);
		}
		else
		{
			Serial.print(g_StrPROBE);
			Serial.print(g_StrENTER);
			Serial.println(ZProbeScreen::PROBE_NEW_TOOL);
		}
	}
	else if (button == BUTTON_BACK)
	{
		CloseScreen();
	}
}
