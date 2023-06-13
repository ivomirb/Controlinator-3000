long g_Counter;

void WelcomeScreen::Draw( void )
{
#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawName = s_DrawState.bDrawAll || strcmp(g_RomSettings.pendantName, pDrawState->name) != 0;
	const bool bDrawConnected = s_DrawState.bDrawAll || pDrawState->bConnected != g_bConnected;
	Strcpy(pDrawState->name, g_RomSettings.pendantName);
	pDrawState->bConnected = g_bConnected;
#else
	const bool bDrawName = true, bDrawConnected = true;
#endif

	if (s_DrawState.bDrawAll)
	{
		DrawTextXY(13, 36, ROMSTR("Connected to PC"));
		DrawTextXY(13, 52, ROMSTR("Connected to CNC"));
		DrawTextXY(1, 52, STRING_UNCHECKED);
	}
	if (bDrawName)
	{
		DrawBox(0, 7, 128, 14);
		SetColorIndex(0);
		int8_t len = Strlen(g_RomSettings.pendantName);
		DrawTextXY(64 - (len*7)/2, 10, g_RomSettings.pendantName);
	}
	if (bDrawConnected)
	{
		SetColorIndex(1);
		DrawTextXY(1, 36, g_bConnected ? STRING_CHECKED : STRING_UNCHECKED);
	}
}

void WelcomeScreen::Update( unsigned long time )
{
	if (g_bConnected && g_MachineStatus != STATUS_DISCONNECTED)
	{
		CloseScreen();
	}
}
