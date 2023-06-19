DEFINE_STRING(g_StrCAL, "CAL:");

void CalibrationScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->stage != m_Stage;
	pDrawState->stage = m_Stage;
	if (!bDrawAll) return;
	if (!s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}
#endif

	if (m_Stage == 0)
	{
		DrawText(0, 0, ROMSTR("[Calibrate Range]"));
		DrawText(2, 1, ROMSTR("Move stick to"));
		DrawText(3, 2, ROMSTR("all corners"));
		DrawText(2, 3, ROMSTR("and press OK"));
		DrawButton(BUTTON_OK, g_StrOK, 2, false);
	}
	else if (m_Stage <= DEADZONE_COUNT)
	{
		DrawText(0, 0, ROMSTR("[Calibrate Center]"));
		DrawText(3, 1, ROMSTR("Move stick,"));
		DrawText(2, 2, ROMSTR("release, then"));
		DrawText(4, 3, ROMSTR("press OK"));
		int8_t len = Sprintf(g_TextBuf, "OK [%d/%d]", m_Stage, DEADZONE_COUNT);
		DrawButton(BUTTON_OK, g_TextBuf, len, false);
	}
	DrawButton(BUTTON_BACK, g_StrBack, 4, false);
	DrawUnusedButtons(0x77);
}

void CalibrationScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (button == BUTTON_OK)
	{
		Serial.print(g_StrCAL);
		Serial.println(m_Stage);
		m_Stage++;
		if (m_Stage > DEADZONE_COUNT)
		{
			m_Stage = -1;
		}
	}
	else if (button == BUTTON_BACK)
	{
		CloseScreen();
	}
}

void CalibrationScreen::Activate( unsigned long time )
{
	BaseScreen::Activate(time);
	SendXYUpdate(true);
	m_Stage = 0;
}

void CalibrationScreen::Deactivate( void )
{
	Serial.print(g_StrCAL);
	Serial.println(g_StrCANCEL);
}

void CalibrationScreen::SendXYUpdate( bool bForce )
{
	if (bForce || (m_OldJoyX != g_JoyX || m_OldJoyY != g_JoyY))
	{
		Serial.print(ROMSTR("RAWJOY:"));
		Serial.print(g_JoyX);
		Serial.print(g_StrComma);
		Serial.println(g_JoyY);
		m_OldJoyX = g_JoyX;
		m_OldJoyY = g_JoyY;
	}
}

void CalibrationScreen::ProcessCommand( const char *command, unsigned long time )
{
	if (strcmp(command, "STARTC") == 0)
	{
		Activate(time);
	}
	else if (strcmp(command, "STOPC") == 0)
	{
		CloseScreen();
	}
	else if (strcmp(command, "STARTJ") == 0)
	{
		m_bSendXY = true;
		SendXYUpdate(true);
	}
	else if (strcmp(command, "STOPJ") == 0)
	{
		m_bSendXY = false;
		CloseScreen();
	}
}
