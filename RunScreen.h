DEFINE_STRING(g_StrJOB, "JOB:");
DEFINE_STRING(g_StrRPM_0, "RPM 0");
DEFINE_STRING(g_StrRPM0, "RPM0");
DEFINE_STRING(g_StrFEED, "FEED:");
DEFINE_STRING(g_StrSPEED, "SPEED:");

RunScreen::ScreenState RunScreen::DecodeState( void ) const
{
	if (g_MachineStatus == STATUS_IDLE)
	{
		return STATE_IDLE;
	}

	if (g_MachineStatus == STATUS_RUN || g_MachineStatus == STATUS_RUNNING || g_MachineStatus == STATUS_RESUMING || g_MachineStatus == STATUS_DOOR3_RESUMING)
	{
		return STATE_RUNNING;
	}

	if (g_MachineStatus == STATUS_HOLD1_STOPPING || g_MachineStatus == STATUS_DOOR2_STOPPING)
	{
		return STATE_PAUSING;
	}

	if (g_MachineStatus == STATUS_DOOR1_OPENED || g_MachineStatus == STATUS_PAUSED)
	{
		return STATE_PAUSED;
	}

	if (g_MachineStatus == STATUS_HOLD0_COMPLETE || g_MachineStatus == STATUS_DOOR0_CLOSED)
	{
		return STATE_PAUSED_READY;
	}

	return STATE_OTHER;
}

void RunScreen::Draw( void )
{
	DrawMachineStatus();
	DrawText(0, 1, g_StrX);
	DrawText(0, 2, g_StrY);
	DrawText(0, 3, g_StrZ);

	PrintX(g_TextBuf); DrawText(2, 1, g_TextBuf);
	PrintY(g_TextBuf); DrawText(2, 2, g_TextBuf);
	PrintZ(g_TextBuf); DrawText(2, 3, g_TextBuf);

	uint8_t unusedButtons = 0x70;

	ScreenState state = DecodeState();
	switch (state)
	{
		case STATE_IDLE: // job hasn't started, ready to run
			{
				DrawButton(BUTTON_RUN, ROMSTR("Run"), 3, true);
				DrawButton(BUTTON_BACK, g_StrBack, 4, false);
				unusedButtons &= ~((1<<BUTTON_RUN)|(1<<BUTTON_BACK));
			}
			break;
		case STATE_RUNNING: // job is currently running
			{
				DrawButton(BUTTON_PAUSE, ROMSTR("Pause"), 5, false);
				DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
				unusedButtons &= ~((1<<BUTTON_PAUSE)|(1<<BUTTON_STOP));
			}
			break;
		case STATE_PAUSED: // job is paused, but not clear to resume
			{
				DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
				unusedButtons &= ~(1<<BUTTON_STOP);
				if (g_RealRpm > 0)
				{
					DrawButton(BUTTON_RPM0, g_StrRPM_0, 5, false);
					unusedButtons &= ~(1<<BUTTON_RPM0);
				}
			}
			break;
		case STATE_PAUSED_READY: // job is ready to resome
			{
				DrawButton(BUTTON_RESUME, ROMSTR("Resume"), 6, true);
				DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
				unusedButtons &= ~((1<<BUTTON_RESUME)|(1<<BUTTON_STOP));
				if (g_RealRpm > 0)
				{
					DrawButton(BUTTON_RPM0, g_StrRPM_0, 5, false);
					unusedButtons &= ~(1<<BUTTON_RPM0);
				}
			}
			break;
		case STATE_OTHER:
			{
			}
			break;
	}
	DrawUnusedButtons(unusedButtons);

	Sprintf(g_TextBuf, "%3d", g_FeedOverride);
	if (m_Override == BUTTON_FEED)
	{
		DrawText(0, 4, g_StrBoldF);
		DrawTextBold(2, 4, g_TextBuf);
		DrawText(5, 4, g_StrBoldPercent);
	}
	else
	{
		DrawText(0, 4, g_StrF);
		DrawText(2, 4, g_TextBuf);
		DrawText(5, 4, g_StrPercent);
	}

	Sprintf(g_TextBuf, "%3d", g_RpmOverride);
	if (m_Override == BUTTON_SPEED)
	{
		DrawText(12, 4, g_StrBoldS);
		DrawTextBold(14, 4, g_TextBuf);
		DrawText(17, 4, g_StrBoldPercent);
	}
	else
	{
		DrawText(12, 4, g_StrS);
		DrawText(14, 4, g_TextBuf);
		DrawText(17, 4, g_StrPercent);
	}
}

void RunScreen::Update( unsigned long time )
{
	int8_t button = GetCurrentButton();
	ScreenState state = DecodeState();

	int16_t wheel = EncoderDrainValue();
	if (m_Override != 0)
	{
		if (wheel != 0)
		{
			m_OverrideTimer = time;
			if (m_Override == BUTTON_SPEED)
			{
				Serial.print(g_StrSPEED);
				Serial.println(wheel);
				return;
			}

			if (m_Override == BUTTON_FEED)
			{
				Serial.print(g_StrFEED);
				Serial.println(wheel);
				return;
			}
		}
		if (time - m_OverrideTimer > OVERRIDE_TIMER)
		{
			m_OverrideTimer = 0;
			m_Override = 0;
		}

		if (TestBit(g_ButtonHold, m_Override))
		{
			Serial.print(m_Override == BUTTON_SPEED ? g_StrSPEED : g_StrFEED);
			Serial.println(0);
			return;
		}
	}

	if (state != STATE_OTHER)
	{
		if (button == BUTTON_SPEED || button == BUTTON_FEED)
		{
			m_Override = button;
			m_OverrideTimer = time;
			return;
		}
	}

	switch (state)
	{
		case STATE_RUNNING: // job is currently running
			{
				if (button == BUTTON_PAUSE)
				{
					Serial.print(g_StrJOB);
					Serial.println(ROMSTR("PAUSE"));
					return;
				}
				if (button == BUTTON_STOP)
				{
					Serial.print(g_StrJOB);
					Serial.println(g_StrSTOP);
					return;
				}
			}
			break;
		case STATE_PAUSED: // job is paused, but not clear to resume
			{
				if (button == BUTTON_STOP)
				{
					Serial.print(g_StrJOB);
					Serial.println(g_StrSTOP);
					return;
				}
				if (button == BUTTON_RPM0)
				{
					Serial.print(g_StrJOB);
					Serial.println(g_StrRPM0);
					return;
				}
			}
			break;
		case STATE_PAUSED_READY: // job is ready to resome
			{
				if (TestBit(g_ButtonHold, BUTTON_RESUME))
				{
					Serial.print(g_StrJOB);
					Serial.println(ROMSTR("RESUME"));
					return;
				}
				if (button == BUTTON_STOP)
				{
					Serial.print(g_StrJOB);
					Serial.println(g_StrSTOP);
					m_JobState = JOB_STOPPED;
					return;
				}
				if (button == BUTTON_RPM0)
				{
					Serial.print(g_StrJOB);
					Serial.println(g_StrRPM0);
					return;
				}
			}
			break;
		case STATE_IDLE: // job hasn't started, ready to run
			{
				if (button == BUTTON_BACK)
				{
					CloseScreen();
					return;
				}
				if (m_JobState == JOB_NOT_STARTED)
				{
					if (TestBit(g_ButtonHold, BUTTON_RUN))
					{
						Serial.print(g_StrJOB);
						Serial.println(g_StrSTART);
						m_JobState = JOB_STARTED;
						return;
					}
				}
				else if (m_JobState == JOB_STOPPED)
				{
					CloseScreen();
					return;
				}
			}
			break;
		case STATE_OTHER:
			{
				CloseScreen();
				return;
			}
			break;
	}
}

void RunScreen::Activate( unsigned long time )
{
	BaseScreen::Activate(time);
	m_Override = 0;
	m_OverrideTimer = 0;
	m_JobState = JOB_NOT_STARTED;
	EncoderDrainValue();
}

void RunScreen::RunDetected( void )
{
	Assert(IsActive());
	m_JobState = JOB_STARTED;
}
