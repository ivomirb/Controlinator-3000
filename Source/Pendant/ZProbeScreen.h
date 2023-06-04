#pragma once

DEFINE_STRING(g_StrPROBE, "PROBE:");

void ZProbeScreen::Draw( void )
{
	DrawMachineStatus();
	uint8_t unusedButtons = 0x78;
	if (m_bJoggingUp)
	{
		u8g2.drawBox(0, g_Rows[2] - 1, 4*7 + 2, 10);
		u8g2.setColorIndex(0);
	}
	DrawText(0, 2, ROMSTR("Z Up"));
	u8g2.setColorIndex(1);
	if (m_bJoggingDown)
	{
		u8g2.drawBox(0, g_Rows[3] - 1, 6*7 + 2, 10);
		u8g2.setColorIndex(0);
	}
	DrawText(0, 3, ROMSTR("Z Down"));
	u8g2.setColorIndex(1);

	if (g_MachineStatus == STATUS_IDLE && m_bConfirmed)
	{
		DrawButton(BUTTON_PROBE, ROMSTR("Probe"), 5, true);
		unusedButtons &= ~(1 << BUTTON_PROBE);
	}

	DrawButton(BUTTON_BACK, g_StrBack, 4, false);
	DrawText(0, 1, m_bConfirmed ? g_StrChecked : g_StrUnchecked);
	if (m_ProbeMode == PROBE_Z)
	{
		DrawText(2, 1, ROMSTR("Connect probe"));
	}
	else
	{
		DrawText(2, 1, ROMSTR("Go to sensor"));
	}

	if (g_bCanShowStop)
	{
		DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
		unusedButtons &= ~(1 << BUTTON_STOP);
	}
	DrawUnusedButtons(unusedButtons);
}

void ZProbeScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (g_MachineStatus == STATUS_IDLE && (time - g_LastBusyTime > 500) && !m_bJoggingUp && !m_bJoggingDown)
	{
		if (TestBit(g_ButtonState, BUTTON_UP))
		{
			Serial.print(g_StrPROBE);
			Serial.println(ROMSTR("Z+"));
			m_bJoggingUp = true;
		}
		else if (TestBit(g_ButtonState, BUTTON_DOWN))
		{
			Serial.print(g_StrPROBE);
			Serial.println(ROMSTR("Z-"));
			m_bJoggingDown = true;
		}
	}
	if ((m_bJoggingUp && !TestBit(g_ButtonState, BUTTON_UP)) || (m_bJoggingDown && !TestBit(g_ButtonState, BUTTON_DOWN)))
	{
		Serial.print(g_StrPROBE);
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
			Serial.print(g_StrPROBE);
			Serial.println(ROMSTR("CONNECT"));
			m_bConfirmed = true;
		}
		else
		{
			Serial.print(g_StrPROBE);
			Serial.println(ROMSTR("GOTOSENSOR"));
		}
	}
	else if (button == BUTTON_BACK)
	{
		Serial.print(g_StrPROBE);
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
		Serial.print(g_StrPROBE);
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
		Serial.print(g_StrPROBE);
		Serial.print(g_StrENTER);
		Serial.println(m_ProbeMode);
	}
	m_bConfirmed = false;
	m_bJoggingUp = false;
	m_bJoggingDown = false;
}

void ZProbeScreen::Deactivate( void )
{
	if (m_bJoggingUp || m_bJoggingDown)
	{
		Serial.print(g_StrPROBE);
		Serial.println(ROMSTR("Z="));
		m_bJoggingUp = false;
		m_bJoggingDown = false;
	}
}
