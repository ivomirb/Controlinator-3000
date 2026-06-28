#pragma once

#if USE_SHARED_STATE
ZProbeScreen::ActiveState *ZProbeScreen::GetActiveState( void )
{
	Assert(IsActive());
	return &g_ScreenTimeshare.zprobe;
}
#endif

void ZProbeScreen::Draw( void )
{
	auto *pState = GetActiveState();

	uint8_t unusedButtons = 0x78;
	if (g_MachineStatus == STATUS_IDLE && pState->m_bConfirmed)
	{
		unusedButtons &= ~(1 << BUTTON_PROBE);
		if (pState->m_ProbeMode == PROBE_Z && (g_ProbeState & PROBE_MEASURE_ENABLED))
		{
			unusedButtons &= ~(1 << BUTTON_MEASURE);
		}
	}
	if (g_bCanShowStop)
	{
		unusedButtons &= ~(1 << BUTTON_STOP);
	}

#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->unusedButtons == unusedButtons;
	const bool bDrawUp = bDrawAll || pDrawState->bJoggingUp != pState->m_bJoggingUp;
	const bool bDrawDown = bDrawAll || pDrawState->bJoggingDown != pState->m_bJoggingDown;
	const bool bDrawFooter = bDrawAll || pDrawState->bContact != g_bProbeContact || pDrawState->bNudging != pState->m_bNudging;
	pDrawState->unusedButtons = unusedButtons;
	pDrawState->bJoggingUp = pState->m_bJoggingUp;
	pDrawState->bJoggingDown = pState->m_bJoggingDown;
	pDrawState->bNudging = pState->m_bNudging;
	pDrawState->bContact = g_bProbeContact;
	if (bDrawAll && !s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}
#else
	const bool bDrawButton = true, bDrawUp = true, bDrawDown = true, bDrawFooter = true, bDrawAll = true;
#endif

	if (bDrawAll)
	{
		DrawMachineStatus(g_StrPROBE, 5);
		DrawButton(BUTTON_BACK, g_StrBack, 4, false);
		if (pState->m_ProbeMode == PROBE_Z)
		{
			DrawText(2, 1, ROMSTR("Connect probe"));
		}
		else
		{
			DrawText(2, 1, ROMSTR("Go to sensor"));
		}

		if (g_MachineStatus == STATUS_IDLE && pState->m_bConfirmed)
		{
			DrawButton(BUTTON_PROBE, ROMSTR("Probe"), 5, true);
			if (pState->m_ProbeMode == PROBE_Z && (g_ProbeState & PROBE_MEASURE_ENABLED))
			{
				DrawButton(BUTTON_MEASURE, ROMSTR("Measure"), 7, true);
			}
		}

		SetDrawColor(1);
		DrawText(0, 1, pState->m_bConfirmed ? g_StrChecked : g_StrUnchecked);

		if (g_bCanShowStop)
		{
			DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
		}
		DrawUnusedButtons(unusedButtons);
	}

	if (bDrawUp)
	{
		if (pState->m_bJoggingUp)
		{
			DrawBox(0, g_Rows[2] - 1, 4*7 + 2, 10);
			SetDrawColor(0);
		}
#if PARTIAL_SCREEN_UPDATE
		else
		{
			SetDrawColor(0);
			DrawBox(0, g_Rows[2] - 1, 4*7 + 2, 10);
			SetDrawColor(1);
		}
#endif
		DrawText(0, 2, ROMSTR("Z Up"));
		SetDrawColor(1);
	}

	if (bDrawDown)
	{
		if (pState->m_bJoggingDown)
		{
			DrawBox(0, g_Rows[3] - 1, 6*7 + 2, 10);
			SetDrawColor(0);
		}
#if PARTIAL_SCREEN_UPDATE
		else
		{
			SetDrawColor(0);
			DrawBox(0, g_Rows[3] - 1, 6*7 + 2, 10);
			SetDrawColor(1);
		}
#endif
		DrawText(0, 3, ROMSTR("Z Down"));
		SetDrawColor(1);
	}

	if (bDrawFooter)
	{
		if (g_bProbeContact)
		{
			DrawBox(8, g_Rows[4] - 1, 7*7 + 1, 10);
			SetDrawColor(0);
			DrawTextXY(9, g_Rows[4], ROMSTR("CONTACT"));
			SetDrawColor(1);
		}
#ifndef DISABLE_ZPROBE_NUDGE
		else if (pState->m_bNudging)
		{
			DrawBox(8, g_Rows[4] - 1, 7*7 + 1, 10);
			SetDrawColor(0);
			DrawTextXY(9, g_Rows[4], ROMSTR("MOVE XY"));
			SetDrawColor(1);
		}
#endif
#if PARTIAL_SCREEN_UPDATE
		else
		{
			SetDrawColor(0);
			DrawBox(8, g_Rows[4] - 1, 7*7 + 1, 10);
			SetDrawColor(1);
		}
#endif
	}
}

void ZProbeScreen::Update( unsigned long time )
{
	auto *pState = GetActiveState();

	int8_t button = GetCurrentButton();
	if (pState->m_bJoggingLocked && !TestBit(g_ButtonState, BUTTON_UP) && !TestBit(g_ButtonState, BUTTON_DOWN))
	{
		// lock joging until both up and down buttons are released to avoid accidental move as the screen is activated
		pState->m_bJoggingLocked = false;
	}

	if (g_ButtonState)
	{
		pState->m_LastInputTime = time;
	}
	if (time - pState->m_LastInputTime > ZPROBE_INACTIVITY_TIMER)
	{
		CloseScreen();
		return;
	}

#ifndef DISABLE_ZPROBE_NUDGE
	if (time - pState->m_LastInputTime > NUDGE_INACTIVITY_TIMER)
	{
		CancelNudge();
	}
#endif

	if (!pState->m_bJoggingLocked && g_MachineStatus == STATUS_IDLE && (time - g_LastBusyTime > 500) && !pState->m_bJoggingUp && !pState->m_bJoggingDown)
	{
		if (TestBit(g_ButtonState, BUTTON_UP))
		{
			Serial.print(g_StrPROBE2);
			Serial.println(g_StrZPlus);
			pState->m_bJoggingUp = true;
		}
		else if (TestBit(g_ButtonState, BUTTON_DOWN))
		{
			Serial.print(g_StrPROBE2);
			Serial.println(g_StrZMinus);
			pState->m_bJoggingDown = true;
		}
	}
	if ((pState->m_bJoggingUp && !TestBit(g_ButtonState, BUTTON_UP)) || (pState->m_bJoggingDown && !TestBit(g_ButtonState, BUTTON_DOWN)))
	{
		CancelJog();
	}

	if (pState->m_ProbeMode != PROBE_Z)
	{
		pState->m_bConfirmed = (g_ProbeState & PROBE_TLO_IN_POSITION) != 0;
	}

	if (button == BUTTON_CONNECT)
	{
		if (pState->m_ProbeMode == PROBE_Z)
		{
			Serial.print(g_StrPROBE2);
			Serial.println(ROMSTR("CONNECT"));
			pState->m_bConfirmed = true;
		}
		else
		{
			Serial.print(g_StrPROBE2);
			Serial.println(ROMSTR("GOTOSENSOR"));
		}
	}
	else if (button == BUTTON_BACK)
	{
		Serial.print(g_StrPROBE2);
		Serial.println(g_StrCANCEL);
		if (g_ProbeState & PROBE_TLO_ENABLED)
		{
			g_ProbeMenuScreen.Activate(time);
		}
		else
		{
			CloseScreen();
		}
	}
	else if (pState->m_bConfirmed && g_MachineStatus == STATUS_IDLE && pState->m_ProbeMode == PROBE_Z && (g_ProbeState & PROBE_MEASURE_ENABLED) && TestBit(g_ButtonHold, BUTTON_MEASURE))
	{
		Serial.print(g_StrPROBE2);
		Serial.print(g_StrSTART);
		Serial.println(PROBE_MEASURE_Z);
		CloseScreen();
	}
	else if (pState->m_bConfirmed && g_MachineStatus == STATUS_IDLE && TestBit(g_ButtonHold, BUTTON_PROBE))
	{
		Serial.print(g_StrPROBE2);
		Serial.print(g_StrSTART);
		Serial.println(pState->m_ProbeMode);
		CloseScreen();
	}
	else if (g_bCanShowStop && button == BUTTON_STOP)
	{
		Serial.println(g_StrSTOP);
	}

#ifndef DISABLE_ZPROBE_NUDGE
	if (pState->m_ProbeMode == PROBE_Z && button == BUTTON_JOYSTICK)
	{
		if (!pState->m_bNudging && !pState->m_bJoggingUp && !pState->m_bJoggingDown)
		{
			pState->m_bNudging = true;
			JogScreen::GetJoystick(&pState->m_OldJoyX, &pState->m_OldJoyY);
		}
		else if (pState->m_bNudging)
		{
			CancelNudge();
		}
	}
	if (pState->m_bNudging)
	{
		// XY nudging
		if (pState->m_bJoggingUp || pState->m_bJoggingDown)
		{
			CancelNudge();
		}
		else
		{
			int8_t x, y;
			JogScreen::GetJoystick(&x, &y);
			if (x != 0 || y != 0)
			{
				pState->m_LastInputTime = time;
			}
			if (pState->m_OldJoyX != x || pState->m_OldJoyY != y)
			{
				int16_t d1 = pState->m_OldJoyX * pState->m_OldJoyX + pState->m_OldJoyY * pState->m_OldJoyY;
				int16_t d2 = x * x + y * y;
				if ((x == 0 && y == 0) || d2 > d1 || time - pState->m_LastJoystickTime >= JOYSTICK_UPDATE_TIME)
				{
					pState->m_OldJoyX = x;
					pState->m_OldJoyY = y;
					pState->m_LastJoystickTime = time;
					Serial.print(g_StrJOG2);
					Sprintf(g_TextBuf, "NXY%d,%d", x, y);
					Serial.println(g_TextBuf);
				}
			}
			else if (x != 0 || y != 0)
			{
				pState->m_LastJoystickTime = time; // reset timer if the joystick hasn't moved
			}
		}
	}
#endif
}

void ZProbeScreen::Activate( unsigned long time, ProbeMode mode, bool bNotify )
{
	BaseScreen::Activate(time);

	auto *pState = GetActiveState();
	pState->m_ProbeMode = mode;
	if (bNotify)
	{
		Serial.print(g_StrPROBE2);
		Serial.print(g_StrENTER);
		Serial.println(pState->m_ProbeMode);
	}
	pState->m_bConfirmed = false;
	pState->m_bJoggingUp = false;
	pState->m_bJoggingDown = false;
	pState->m_bJoggingLocked = true;
	pState->m_bNudging = false;
	pState->m_LastInputTime = time;

#ifndef DISABLE_ZPROBE_NUDGE
	pState->m_LastJoystickTime = time;
	pState->m_OldJoyX = pState->m_OldJoyY = 0;
#endif
}

void ZProbeScreen::Deactivate( void )
{
	CancelJog();
#ifndef DISABLE_ZPROBE_NUDGE
	CancelNudge();
#endif
}

void ZProbeScreen::CancelJog( void )
{
	auto *pState = GetActiveState();

	if (pState->m_bJoggingUp || pState->m_bJoggingDown)
	{
		Serial.print(g_StrPROBE2);
		Serial.println(g_StrZStop);
		pState->m_bJoggingUp = false;
		pState->m_bJoggingDown = false;
	}
}

#ifndef DISABLE_ZPROBE_NUDGE
void ZProbeScreen::CancelNudge( void )
{
	auto *pState = GetActiveState();
	if (pState->m_bNudging)
	{
		Serial.print(g_StrJOG2);
		Serial.println(ROMSTR("NXY0,0"));
		pState->m_bNudging = false;
	}
}
#endif
