#pragma once

void ZProbeScreen::Draw( void )
{
	uint8_t unusedButtons = 0x78;
	if (g_MachineStatus == STATUS_IDLE && m_bConfirmed)
	{
		unusedButtons &= ~(1 << BUTTON_PROBE);
	}
	if (g_bCanShowStop)
	{
		unusedButtons &= ~(1 << BUTTON_STOP);
	}

#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->unusedButtons == unusedButtons;
	const bool bDrawUp = bDrawAll || pDrawState->bJoggingUp != m_bJoggingUp;
	const bool bDrawDown = bDrawAll || pDrawState->bJoggingDown != m_bJoggingDown;
	pDrawState->unusedButtons = unusedButtons;
	pDrawState->bJoggingUp = m_bJoggingUp;
	pDrawState->bJoggingDown = m_bJoggingDown;
	if (bDrawAll && !s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}
#else
	const bool bDrawButton = true, bDrawUp = true, bDrawDown = true, bDrawAll = true;
#endif

	if (bDrawAll)
	{
		DrawMachineStatus(g_StrPROBE, 5);
		DrawButton(BUTTON_BACK, g_StrBack, 4, false);
		if (m_ProbeMode == PROBE_Z)
		{
			DrawText(2, 1, ROMSTR("Connect probe"));
		}
		else
		{
			DrawText(2, 1, ROMSTR("Go to sensor"));
		}

		if (g_MachineStatus == STATUS_IDLE && m_bConfirmed)
		{
			DrawButton(BUTTON_PROBE, ROMSTR("Probe"), 5, true);
		}

		DrawText(0, 1, m_bConfirmed ? g_StrChecked : g_StrUnchecked);

		if (g_bCanShowStop)
		{
			DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
		}
		DrawUnusedButtons(unusedButtons);
	}

	if (bDrawUp)
	{
		if (m_bJoggingUp)
		{
			DrawBox(0, g_Rows[2] - 1, 4*7 + 2, 10);
			SetColorIndex(0);
		}
#if PARTIAL_SCREEN_UPDATE
		else
		{
			SetColorIndex(0);
			DrawBox(0, g_Rows[2] - 1, 4*7 + 2, 10);
			SetColorIndex(1);
		}
#endif
		DrawText(0, 2, ROMSTR("Z Up"));
		SetColorIndex(1);
	}

	if (bDrawDown)
	{
		if (m_bJoggingDown)
		{
			DrawBox(0, g_Rows[3] - 1, 6*7 + 2, 10);
			SetColorIndex(0);
		}
#if PARTIAL_SCREEN_UPDATE
		else
		{
			SetColorIndex(0);
			DrawBox(0, g_Rows[3] - 1, 6*7 + 2, 10);
			SetColorIndex(1);
		}
#endif
		DrawText(0, 3, ROMSTR("Z Down"));
		SetColorIndex(1);
	}
}

void ZProbeScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (m_bJoggingLocked && !TestBit(g_ButtonState, BUTTON_UP) && !TestBit(g_ButtonState, BUTTON_DOWN))
	{
		// lock joging until both up and down buttons are released to avoid accidental move as the screen is activated
		m_bJoggingLocked = false;
	}

	if (!m_bJoggingLocked && g_MachineStatus == STATUS_IDLE && (time - g_LastBusyTime > 500) && !m_bJoggingUp && !m_bJoggingDown)
	{
		if (TestBit(g_ButtonState, BUTTON_UP))
		{
			Serial.print(g_StrPROBE2);
			Serial.println(ROMSTR("Z+"));
			m_bJoggingUp = true;
		}
		else if (TestBit(g_ButtonState, BUTTON_DOWN))
		{
			Serial.print(g_StrPROBE2);
			Serial.println(ROMSTR("Z-"));
			m_bJoggingDown = true;
		}
	}
	if ((m_bJoggingUp && !TestBit(g_ButtonState, BUTTON_UP)) || (m_bJoggingDown && !TestBit(g_ButtonState, BUTTON_DOWN)))
	{
		Serial.print(g_StrPROBE2);
		Serial.println(ROMSTR("Z="));
		m_bJoggingUp = m_bJoggingDown = false;
	}

	if (m_ProbeMode != PROBE_Z)
	{
		m_bConfirmed = (g_TloState & TLO_IN_POSITION) != 0;
	}

	if (button == BUTTON_CONNECT)
	{
		if (m_ProbeMode == PROBE_Z)
		{
			Serial.print(g_StrPROBE2);
			Serial.println(ROMSTR("CONNECT"));
			m_bConfirmed = true;
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
		if (g_TloState & TLO_ENABLED)
		{
			g_ProbeMenuScreen.Activate(time);
		}
		else
		{
			CloseScreen();
		}
	}
	else if (m_bConfirmed && g_MachineStatus == STATUS_IDLE && TestBit(g_ButtonHold, BUTTON_PROBE))
	{
		Serial.print(g_StrPROBE2);
		Serial.print(g_StrSTART);
		Serial.println(m_ProbeMode);
		CloseScreen();
	}
	else if (g_bCanShowStop && button == BUTTON_STOP)
	{
		Serial.println(g_StrSTOP);
	}
}

void ZProbeScreen::Activate( unsigned long time, ProbeMode mode, bool bNotify )
{
	BaseScreen::Activate(time);
	m_ProbeMode = mode;
	if (bNotify)
	{
		Serial.print(g_StrPROBE2);
		Serial.print(g_StrENTER);
		Serial.println(m_ProbeMode);
	}
	m_bConfirmed = false;
	m_bJoggingUp = false;
	m_bJoggingDown = false;
	m_bJoggingLocked = true;
}

void ZProbeScreen::Deactivate( void )
{
	if (m_bJoggingUp || m_bJoggingDown)
	{
		Serial.print(g_StrPROBE2);
		Serial.println(ROMSTR("Z="));
		m_bJoggingUp = false;
		m_bJoggingDown = false;
	}
}
