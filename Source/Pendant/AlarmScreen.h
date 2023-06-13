void AlarmScreen::Activate( unsigned long time )
{
	BaseScreen::Activate(time);
	m_bDismissed = false;
	m_DismissTime = 0;
}

void AlarmScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	uint8_t state = 4;
	if (m_bDismissed)
	{
		state = ((g_CurrentTime - m_DismissTime) / 200) % 3;
	}
	const bool bDrawButton = s_DrawState.bDrawAll || pDrawState->state != state;
	pDrawState->state = state;
#else
	const bool bDrawButton = true;
#endif

	if (s_DrawState.bDrawAll)
	{
		DrawText(5, 0, ROMSTR("[Alarm]"));
		DrawUnusedButtons(0x7F);
		DrawText(3, 1, ROMSTR("Check the PC"));
		DrawText(3, 2, ROMSTR("for details"));
	}

	if (bDrawButton)
	{
		if (m_bDismissed)
		{
#if PARTIAL_SCREEN_UPDATE
			DrawButton(BUTTON_DISMISS, ROMSTR("       "), 7, false); // clear background
#endif
			DrawText(14 + ((g_CurrentTime - m_DismissTime) / 200) % 3, 4, g_StrPlaceholder);
		}
		else
		{
			DrawButton(BUTTON_DISMISS, ROMSTR("Dismiss"), 7, false);
		}
	}
}

void AlarmScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	if (m_DismissTime == 0 && button == BUTTON_DISMISS)
	{
		Serial.println(ROMSTR("DISMISS"));
		m_bDismissed = true;
		m_DismissTime = time;
	}

	if (g_MachineStatus != STATUS_ALARM && !m_bDismissed && m_DismissTime == 0)
	{
		m_DismissTime = time; // if the alarm cleared on its own, start the dimiss timer
	}

	if (m_DismissTime && time - m_DismissTime > DISMISS_TIMER)
	{
		// after 1 second, either close the screen or reenable the button
		m_bDismissed = false;
		m_DismissTime = 0;
		if (g_MachineStatus != STATUS_ALARM)
		{
			CloseScreen();
		}
	}
}
