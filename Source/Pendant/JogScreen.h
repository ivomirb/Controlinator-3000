const int WHEEL_UPDATE_TIME = 100; // don't send wheel updates more than once every 100ms
const int JOYSTICK_UPDATE_TIME = 100; // don't send joystick updates more than once every 100ms

const char g_AxisName[5] = {' ', 'X', 'Y', ' ', 'Z'};

#if USE_SHARED_STATE
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
	m_StepRateCount = 2;
	m_StepRates[0] = 10;
	m_StepRates[1] = 100;
}

void JogScreen::Draw( void )
{
	auto *pState = GetActiveState();

#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->bWorkSpace != g_bWorkSpace ||
		pDrawState->bShowInches != g_bShowInches || pDrawState->axis != pState->m_Axis ||
		pDrawState->bShowStop != pState->m_bShowStop || pDrawState->bShowActions != pState->m_bShowActions ||
		pDrawState->bShowAlign != pState->m_bShowAlign || pDrawState->stepIndex != m_StepIndex;
	pDrawState->bWorkSpace = g_bWorkSpace;
	pDrawState->bShowInches = g_bShowInches;
	pDrawState->axis = pState->m_Axis;
	pDrawState->bShowStop = pState->m_bShowStop;
	pDrawState->bShowActions = pState->m_bShowActions;
	pDrawState->bShowAlign = pState->m_bShowAlign;
	pDrawState->stepIndex = m_StepIndex;
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
		DrawMachineStatus(g_StrJOG, 3);
		DrawText(13, 1, g_bWorkSpace ? g_StrWCS : g_StrMCS);
		if ((pState->m_Axis & (pState->m_Axis-1)) == 0)
		{
			// only one axis is selected
			uint16_t step = m_StepRates[m_StepIndex];
			if (pState->m_bShowAlign)
			{
				DrawButton(BUTTON_STEP, ROMSTR("Align"), 5, true);
			}
			else
			{
				int8_t len = g_bShowInches ? Sprintf(g_TextBuf, "Step %1d.%03d", step/1000, step%1000) : Sprintf(g_TextBuf, "Step %2d.%02d", step/100, step%100);
				DrawButton(BUTTON_STEP, g_TextBuf, len, false);
			}

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

	if (bDrawX)
	{
		PrintX(g_TextBuf);
		if (pState->m_Axis & 1)
		{
			SetDrawColor(1);
			DrawBox(0, g_Rows[1] - 1, 8, 10);
			SetDrawColor(0);
			DrawText(0, 1, g_StrBoldX);
			SetDrawColor(1);
			DrawTextBold(2, 1, g_TextBuf);
		}
		else
		{
			SetDrawColor(1);
			DrawText(0, 1, g_StrX);
			DrawText(2, 1, g_TextBuf);
		}
	}

	if (bDrawY)
	{
		PrintY(g_TextBuf);
		if (pState->m_Axis & 2)
		{
			SetDrawColor(1);
			DrawBox(0, g_Rows[2] - 1, 8, 10);
			SetDrawColor(0);
			DrawText(0, 2, g_StrBoldY);
			SetDrawColor(1);
			DrawTextBold(2, 2, g_TextBuf);
		}
		else
		{
			SetDrawColor(1);
			DrawText(0, 2, g_StrY);
			DrawText(2, 2, g_TextBuf);
		}
	}

	if (bDrawZ)
	{
		PrintZ(g_TextBuf);
		if (pState->m_Axis & 4)
		{
			SetDrawColor(1);
			DrawBox(0, g_Rows[3] - 1, 8, 10);
			SetDrawColor(0);
			DrawText(0, 3, g_StrBoldZ);
			SetDrawColor(1);
			DrawTextBold(2, 3, g_TextBuf);
		}
		else
		{
			SetDrawColor(1);
			DrawText(0, 3, g_StrZ);
			DrawText(2, 3, g_TextBuf);
		}
	}
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
		if (g_MachineStatus != STATUS_IDLE)
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
		bool bShowAlign = pState->m_StepHoldTime != 0 && time - pState->m_StepHoldTime > BUTTON_HOLD_TIME / 2;
		if (bShowAlign && !pState->m_bShowAlign)
		{
			g_ButtonChangeTimers[BUTTON_STEP] = 0;
		}
		pState->m_bShowAlign = bShowAlign;
		// only one axis is selected
		if (button == BUTTON_STEP)
		{
			pState->m_StepHoldTime = time;
		}
		if (pState->m_bShowAlign && TestBit(g_ButtonHold, BUTTON_STEP))
		{
			// Step button held down for full time, align to the step rate
			Serial.print(g_StrJOG2);
			uint16_t step = m_StepRates[m_StepIndex];
			if (g_bShowInches)
			{
				Sprintf(g_TextBuf, "AI%c%c%d.%03d", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis], step/1000, step%1000);
			}
			else
			{
				Sprintf(g_TextBuf, "AM%c%c%d.%02d", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis], step/100, step%100);
			}
			Serial.println(g_TextBuf);
		}
		else if (!pState->m_bShowAlign && TestBit(g_ButtonUnclick, BUTTON_STEP))
		{
			m_StepIndex = (m_StepIndex + 1) % m_StepRateCount;
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
				Serial.print(g_StrJOG2);
				Sprintf(g_TextBuf, "0%c%c", g_bWorkSpace ? 'L' : 'G', g_AxisName[pState->m_Axis]);
				Serial.println(g_TextBuf);
			}
		}
		else if (pState->m_bShowStop && button == BUTTON_STOP)
		{
			Serial.println(g_StrSTOP);
		}

		// process wheel, but not too frequently
		if (time - pState->m_LastWheelTime >= WHEEL_UPDATE_TIME)
		{
			int16_t wheel = EncoderDrainValue();
			if (wheel != 0)
			{
				pState->m_LastInputTime = time;
				pState->m_LastWheelTime = time;
				if (g_MachineStatus == STATUS_IDLE || g_MachineStatus == STATUS_JOG || g_MachineStatus == STATUS_RUNNING)
				{
					uint16_t step = m_StepRates[m_StepIndex];
					if (g_bShowInches)
					{
						Sprintf(g_TextBuf, "WI%c%d*%d.%03d", g_AxisName[pState->m_Axis], wheel, step/1000, step%1000);
					}
					else
					{
						Sprintf(g_TextBuf, "WM%c%d*%d.%02d", g_AxisName[pState->m_Axis], wheel, step/100, step%100);
					}
					Serial.print(g_StrJOG2);
					Serial.println(g_TextBuf);
				}
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
			int16_t d1 = pState->m_OldJoyX * pState->m_OldJoyX + pState->m_OldJoyY * pState->m_OldJoyY;
			int16_t d2 = x * x + y * y;
			if ((x == 0 && y == 0) || d2 > d1 || time - pState->m_LastJoystickTime >= JOYSTICK_UPDATE_TIME)
			{
				pState->m_OldJoyX = x;
				pState->m_OldJoyY = y;
				pState->m_LastJoystickTime = time;
				Serial.print(g_StrJOG2);
				Sprintf(g_TextBuf, "JXY%d,%d", x, y);
				Serial.println(g_TextBuf);
			}
		}
		else if (x != 0 || y != 0)
		{
			pState->m_LastJoystickTime = time; // reset timer if the joystick hasn't moved
		}
	}

	if (!TestBit(g_ButtonState, BUTTON_STEP))
	{
		pState->m_StepHoldTime = 0;
		pState->m_bShowAlign = false;
	}
}

void JogScreen::Activate( unsigned long time )
{
	BaseScreen::Activate(time);
	auto *pState = GetActiveState();
	pState->m_LastInputTime = time;
	pState->m_StepHoldTime = 0;
	pState->m_LastWheelTime = time;
	pState->m_LastJoystickTime = time;
	pState->m_bShowStop = false;
	pState->m_bShowActions = true;
	pState->m_bShowAlign = false;
	GetJoystick(&pState->m_OldJoyX, &pState->m_OldJoyY);
	EncoderDrainValue();
}

// Parses the jog step rate string from the PC - |<rate1>|<rate2> ... - up to 5
void JogScreen::ParseJogSteps( const char *str )
{
	m_StepRateCount = 0;
	while (str && m_StepRateCount < 5)
	{
		m_StepRates[m_StepRateCount++] = atol(str + 1);
		str = strchr(str + 1, '|');
	}
	if (m_StepRateCount == 0)
	{
		m_StepRates[0] = 10;
		m_StepRates[1] = 100;
		m_StepRateCount = 2;
	}

	if (m_StepIndex > m_StepRateCount - 1)
	{
		m_StepIndex = m_StepRateCount - 1;
	}
#if PARTIAL_SCREEN_UPDATE
	s_DrawState.bDrawAll = true;
#endif
}

void JogScreen::GetJoystick( int8_t *px, int8_t *py )
{
	*px = QuantizeJoystick(g_JoyX, g_RomSettings.calibration);
	*py = QuantizeJoystick(g_JoyY, g_RomSettings.calibration + 4);
}
