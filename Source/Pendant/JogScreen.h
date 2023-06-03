const char g_AxisName[5] = {' ', 'X', 'Y', ' ', 'Z'};

#ifdef USE_SHARED_STATE
JogScreen::ActiveState *JogScreen::GetActiveState( void )
{
	Assert(IsActive());
	return &g_ScreenTimeshare.jog;
}
#endif

void JogScreen::SetAxis( uint8_t axis )
{
	GetActiveState()->m_Axis = axis;
}

JogScreen::JogScreen( void )
{
	m_JogRateCount = 2;
	m_JogRates[0] = 10;
	m_JogRates[1] = 100;
}

void JogScreen::Draw( void )
{
	auto *pState = GetActiveState();
	DrawMachineStatus();

	PrintX(g_TextBuf);
	if (pState->m_Axis & 1)
	{
		u8g2.setColorIndex(1);
		u8g2.drawBox(0, g_Rows[1] - 1, 8, 10);
		u8g2.setColorIndex(0);
		DrawText(0, 1, g_StrBoldX);
		u8g2.setColorIndex(1);
		DrawTextBold(2, 1, g_TextBuf);
	}
	else
	{
		u8g2.setColorIndex(1);
		DrawText(0, 1, g_StrX);
		DrawText(2, 1, g_TextBuf);
	}

	PrintY(g_TextBuf);
	if (pState->m_Axis & 2)
	{
		u8g2.setColorIndex(1);
		u8g2.drawBox(0, g_Rows[2] - 1, 8, 10);
		u8g2.setColorIndex(0);
		DrawText(0, 2, g_StrBoldY);
		u8g2.setColorIndex(1);
		DrawTextBold(2, 2, g_TextBuf);
	}
	else
	{
		u8g2.setColorIndex(1);
		DrawText(0, 2, g_StrY);
		DrawText(2, 2, g_TextBuf);
	}

	PrintZ(g_TextBuf);
	if (pState->m_Axis & 4)
	{
		u8g2.setColorIndex(1);
		u8g2.drawBox(0, g_Rows[3] - 1, 8, 10);
		u8g2.setColorIndex(0);
		DrawText(0, 3, g_StrBoldZ);
		u8g2.setColorIndex(1);
		DrawTextBold(2, 3, g_TextBuf);
	}
	else
	{
		u8g2.setColorIndex(1);
		DrawText(0, 3, g_StrZ);
		DrawText(2, 3, g_TextBuf);
	}

	DrawText(13, 1, g_bWorkSpace ? g_StrWCS : g_StrMCS);
	if ((pState->m_Axis & (pState->m_Axis-1)) == 0)
	{
		// only one axis is selected
		uint16_t rate = m_JogRates[m_Rate];
		const char *verb = pState->m_bShowRound ? "Mul " : "Rate ";
		int8_t len = g_bShowInches ? Sprintf(g_TextBuf, "%s%1d.%03d", verb, rate/1000, rate%1000) : Sprintf(g_TextBuf, "%s%2d.%02d", verb, rate/100, rate%100);
		DrawButton(BUTTON_RATE, g_TextBuf, len, pState->m_bShowRound);

		if (pState->m_bShowActions)
		{
			DrawButton(BUTTON_GOTO0, ROMSTR("To 0"), 4, true);
			if (g_bWorkSpace)
			{
				DrawButton(BUTTON_SET0, ROMSTR("Set 0"), 5, true);
			}
			else
			{
				DrawUnusedButtons(0x40);
			}
		}
		else if (pState->m_bShowStop)
		{
			DrawButton(BUTTON_STOP, g_StrSTOP, 4, false);
			DrawUnusedButtons(0x40);
		}
		else
		{
			DrawUnusedButtons(0x60);
		}
	}
	else
	{
		// XY selected
		DrawUnusedButtons(0x68);
	}
	DrawButton(BUTTON_BACK, g_StrBack, 4, false);
}

void JogScreen::Update( unsigned long time )
{
	auto *pState = GetActiveState();
	if (g_MachineStatus < STATUS_IDLE && g_MachineStatus != STATUS_JOG && g_MachineStatus != STATUS_RUNNING)
	{
		CloseScreen();
		return;
	}

	// must be before setting m_LastInputTime, otherwise the STOP button will dismiss itself
	pState->m_bShowStop = g_bCanShowStop && (time - pState->m_LastInputTime > SHOW_STOP_TIME); // half second of no idle and no input
	if (pState->m_bShowActions)
	{
		if (g_MachineStatus < STATUS_IDLE)
		{
			pState->m_bShowActions = false; // hide actions as soon as not idle
		}
	}
	else
	{
		if (time - g_LastBusyTime > 250)
		{
			pState->m_bShowActions = true; // delay showing the actions by 250ms to reduce flicker
		}
	}

	if (g_ButtonState != 0)
	{
		pState->m_LastInputTime = time;
	}

	if (time - pState->m_LastInputTime > JOG_INACTIVITY_TIMER)
	{
		CloseScreen();
		return;
	}

	int8_t button = GetCurrentButton();
	if (button == BUTTON_X)
	{
		SetAxis(TestBit(g_ButtonState, BUTTON_Y) ? 3 : 1);
	}
	else if (button == BUTTON_Y)
	{
		SetAxis(TestBit(g_ButtonState, BUTTON_X) ? 3 : 2);
	}
	else if (button == BUTTON_Z)
	{
		SetAxis(4);
	}
	else if (button == BUTTON_JOYSTICK)
	{
		SetAxis(3);
	}
	else if (button == BUTTON_WCS)
	{
		g_bWorkSpace = !g_bWorkSpace;
	}
	else if (button == BUTTON_BACK)
	{
		CloseScreen();
		return;
	}

	if ((pState->m_Axis & (pState->m_Axis-1)) == 0)
	{
		pState->m_bShowRound = pState->m_RateDownTime != 0 && time - pState->m_RateDownTime > BUTTON_HOLD_TIME / 2;
		// only one axis is selected
		if (button == BUTTON_RATE)
		{
			pState->m_RateDownTime = time;
		}
		if (pState->m_bShowRound && TestBit(g_ButtonHold, BUTTON_RATE))
		{
			// Rate button held down for full time, round by the rate
			Serial.print(g_StrJOG);
			uint16_t rate = m_JogRates[m_Rate];
			if (g_bShowInches)
			{
				Sprintf(g_TextBuf, "RI%c%c%d.%03d", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis], rate/1000, rate%1000);
			}
			else
			{
				Sprintf(g_TextBuf, "RM%c%c%d.%02d", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis], rate/100, rate%100);
			}
			Serial.println(g_TextBuf);
		}
		else if (!pState->m_bShowRound && TestBit(g_ButtonUnclick, BUTTON_RATE))
		{
			m_Rate = (m_Rate + 1) % m_JogRateCount;
		}

		if (pState->m_bShowActions && g_MachineStatus == STATUS_IDLE)
		{
			if (TestBit(g_ButtonHold, BUTTON_SET0) && g_bWorkSpace)
			{
				Sprintf(g_TextBuf, "SET0:%c", g_AxisName[pState->m_Axis]);
				Serial.println(g_TextBuf);
			}
			else if (TestBit(g_ButtonHold, BUTTON_GOTO0))
			{
				Serial.print(g_StrJOG);
				Sprintf(g_TextBuf, "0%c%c", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis]);
				Serial.println(g_TextBuf);
			}
		}
		else if (pState->m_bShowStop && button == BUTTON_STOP)
		{
			Serial.println(g_StrSTOP);
		}

		// process wheel
		int16_t wheel = EncoderDrainValue();
		if (wheel != 0)
		{
			pState->m_LastInputTime = time;
			if (g_MachineStatus == STATUS_IDLE || g_MachineStatus == STATUS_JOG || g_MachineStatus == STATUS_RUNNING)
			{
				uint16_t rate = m_JogRates[m_Rate];
				if (g_bShowInches)
				{
					Sprintf(g_TextBuf, "WI%c%d*%d.%03d", g_AxisName[pState->m_Axis], wheel, rate/1000, rate%1000);
				}
				else
				{
					Sprintf(g_TextBuf, "WM%c%d*%d.%02d", g_AxisName[pState->m_Axis], wheel, rate/100, rate%100);
				}
				Serial.print(g_StrJOG);
				Serial.println(g_TextBuf);
			}
		}
	}
	else
	{
		// XY jogging
		int8_t x, y;
		GetJoystick(&x, &y);
		if (x != 0 || y != 0)
		{
			pState->m_LastInputTime = time;
		}
		if (pState->m_OldJoyX != x || pState->m_OldJoyY != y)
		{
			pState->m_OldJoyX = x;
			pState->m_OldJoyY = y;
			Serial.print(g_StrJOG);
			Sprintf(g_TextBuf, "JXY%d,%d", x, y);
			Serial.println(g_TextBuf);
		}
	}

	if (!TestBit(g_ButtonState, BUTTON_RATE))
	{
		pState->m_RateDownTime = 0;
		pState->m_bShowRound = false;
	}
}

void JogScreen::Activate( unsigned long time )
{
	BaseScreen::Activate(time);
	auto *pState = GetActiveState();
	pState->m_LastInputTime = time;
	pState->m_RateDownTime = 0;
	pState->m_bShowStop = false;
	pState->m_bShowActions = true;
	pState->m_bShowRound = false;
	GetJoystick(&pState->m_OldJoyX, &pState->m_OldJoyY);
	EncoderDrainValue();
}

// Parses the jog rate string from the PC - |<rate1>|<rate2> ... - up to 5
void JogScreen::ParseJogRates( const char *str )
{
	m_JogRateCount = 0;
	while (str && m_JogRateCount < 5)
	{
		m_JogRates[m_JogRateCount++] = atol(str + 1);
		str = strchr(str + 1, '|');
	}
	if (m_JogRateCount == 0)
	{
		m_JogRates[0] = 10;
		m_JogRates[1] = 100;
		m_JogRateCount = 2;
	}
}

void JogScreen::GetJoystick( int8_t *px, int8_t *py )
{
	*px = QuantizeJoystick(g_JoyX, g_RomSettings.calibration);
	*py = QuantizeJoystick(g_JoyY, g_RomSettings.calibration + 4);
}
