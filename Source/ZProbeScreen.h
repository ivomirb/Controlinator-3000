#pragma once

DEFINE_STRING(g_StrPROBE, "PROBE:");

void ZProbeScreen::Draw( void )
{
	DrawMachineStatus();
	uint8_t unusedButtons = 0x78;
	if (m_bConfirmed && g_MachineStatus == STATUS_IDLE)
	{
		DrawButton(BUTTON_PROBE, ROMSTR("Probe"), 5, true);
		unusedButtons &= ~(1 << BUTTON_PROBE);
	}
	DrawButton(BUTTON_UP, ROMSTR("Z Up"), 4, false);
	DrawButton(BUTTON_DOWN, ROMSTR("Z Down"), 6, false);
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
	if (button == BUTTON_UP)
	{
		Serial.print(g_StrPROBE);
		Serial.println(ROMSTR("Z+"));
		m_bJogging = true;
	}
	else if (button == BUTTON_DOWN)
	{
		Serial.print(g_StrPROBE);
		Serial.println(ROMSTR("Z-"));
		m_bJogging = true;
	}
	else if (m_bJogging && !TestBit(g_ButtonState, BUTTON_UP) && !TestBit(g_ButtonState, BUTTON_DOWN))
	{
		Serial.print(g_StrPROBE);
		Serial.println(ROMSTR("Z="));
		m_bJogging = false;
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

void ZProbeScreen::Activate( unsigned long time, ProbeMode mode )
{
	BaseScreen::Activate(time);
	m_ProbeMode = mode;
	Serial.print(g_StrPROBE);
	Serial.print(g_StrENTER);
	Serial.println(m_ProbeMode);
	m_bConfirmed = false;
	m_bJogging = false;
}

void ZProbeScreen::Deactivate( void )
{
	if (m_bJogging)
	{
		Serial.print(g_StrPROBE);
		Serial.println(ROMSTR("Z="));
		m_bJogging = false;
	}
}
