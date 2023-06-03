DEFINE_STRING(g_StrJob, "Job>");

void MainScreen::Draw( void )
{
	DrawMachineStatus();
	DrawText(0, 1, g_StrX);
	DrawText(0, 2, g_StrY);
	DrawText(0, 3, g_StrZ);

	PrintX(g_TextBuf); DrawText(2, 1, g_TextBuf);
	PrintY(g_TextBuf); DrawText(2, 2, g_TextBuf);
	PrintZ(g_TextBuf); DrawText(2, 3, g_TextBuf);

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
				g_ZProbeScreen.Activate(time, ZProbeScreen::PROBE_Z);
			}
		}
		else if (button == BUTTON_JOB)
		{
			Serial.println(ROMSTR("JOBMENU"));
		}
		else if (button == BUTTON_MACROS)
		{
			g_MacroScreen.Activate(time);
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
