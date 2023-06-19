DEFINE_STRING(g_StrJob, "Job>");

void MainScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->bWorkSpace != g_bWorkSpace || pDrawState->bShowInches != g_bShowInches ||
		pDrawState->bCanShowStop != g_bCanShowStop || pDrawState->bJobRunning != g_bJobRunning;
	pDrawState->bWorkSpace = g_bWorkSpace;
	pDrawState->bShowInches = g_bShowInches;
	pDrawState->bCanShowStop = g_bCanShowStop;
	pDrawState->bJobRunning = g_bJobRunning;
	if (bDrawAll && !s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}

	const bool bDrawX = bDrawAll || pDrawState->wx != g_WorkX || pDrawState->ox != g_OffsetX;
	const bool bDrawY = bDrawAll || pDrawState->wy != g_WorkY || pDrawState->oy != g_OffsetY;
	const bool bDrawZ = bDrawAll || pDrawState->wz != g_WorkZ || pDrawState->oz != g_OffsetZ;
	pDrawState->wx = g_WorkX;
	pDrawState->ox = g_OffsetX;
	pDrawState->wy = g_WorkY;
	pDrawState->oy = g_OffsetY;
	pDrawState->wz = g_WorkZ;
	pDrawState->oz = g_OffsetZ;
#else
	const bool bDrawX = true, bDrawY = true, bDrawZ = true, bDrawAll = true;
#endif

	if (bDrawAll)
	{
		DrawMachineStatus(ROMSTR("MAIN"), 4);
		DrawText(0, 1, g_StrX);
		DrawText(0, 2, g_StrY);
		DrawText(0, 3, g_StrZ);

		DrawButton(BUTTON_WCS, g_bWorkSpace ? g_StrWCS : g_StrMCS, 5, false);
		if (g_MachineStatus == STATUS_IDLE)
		{
			DrawButton(BUTTON_PROBE, ROMSTR("Probe>"), 6, false);
			DrawButton(BUTTON_JOB, g_StrJob, 4, false);
			DrawButton(BUTTON_MACROS, ROMSTR("Macros>"), 7, false);
			DrawButton(BUTTON_HOME, ROMSTR("Home"), 4, true);
		}
		else
		{
			uint8_t unusedButtons = 0xE8;
			if (g_bCanShowStop)
			{
				DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
				unusedButtons &= ~(1 << BUTTON_STOP);
			}
			if (g_bJobRunning)
			{
				DrawButton(BUTTON_JOB, g_StrJob, 4, false);
				unusedButtons &= ~(1 << BUTTON_JOB);
			}
			DrawUnusedButtons(unusedButtons);
		}
	}

	u8g2.setColorIndex(1);
	if (bDrawX)
	{
		PrintX(g_TextBuf);
		DrawText(2, 1, g_TextBuf);
	}
	if (bDrawY)
	{
		PrintY(g_TextBuf);
		DrawText(2, 2, g_TextBuf);
	}
	if (bDrawZ)
	{
		PrintZ(g_TextBuf);
		DrawText(2, 3, g_TextBuf);
	}
}

void MainScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (button == BUTTON_WCS)
	{
		g_bWorkSpace = !g_bWorkSpace;
	}
	else if (g_MachineStatus == STATUS_IDLE)
	{
		if (button == BUTTON_X)
		{
			g_JogScreen.Activate(time);
			g_JogScreen.SetAxis(1);
		}
		else if (button == BUTTON_Y)
		{
			g_JogScreen.Activate(time);
			g_JogScreen.SetAxis(2);
		}
		else if (button == BUTTON_Z)
		{
			g_JogScreen.Activate(time);
			g_JogScreen.SetAxis(4);
		}
		else if (button == BUTTON_JOYSTICK)
		{
			g_JogScreen.Activate(time);
			g_JogScreen.SetAxis(3);
		}
		else if (TestBit(g_ButtonHold, BUTTON_HOME))
		{
			Serial.println(ROMSTR("HOME"));
		}
		else if (button == BUTTON_PROBE)
		{
			if (g_TloState & TLO_ENABLED)
			{
				g_ProbeMenuScreen.Activate(time);
			}
			else
			{
				g_ZProbeScreen.Activate(time, ZProbeScreen::PROBE_Z, true);
			}
		}
		else if (button == BUTTON_JOB)
		{
			Serial.println(ROMSTR("JOBMENU"));
		}
		else if (button == BUTTON_MACROS)
		{
#ifndef DISABLE_MACRO_SCREEN
			g_MacroScreen.Activate(time);
#endif
		}
	}
	else if (g_bCanShowStop && button == BUTTON_STOP)
	{
		Serial.println(g_StrSTOP);
	}
	else if (g_bJobRunning && button == BUTTON_JOB)
	{
		g_RunScreen.Activate(time);
	}
}
