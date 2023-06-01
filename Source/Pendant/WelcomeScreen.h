void WelcomeScreen::Draw( void )
{
	DrawTextXY(13, 36, ROMSTR("Connected to PC"));
	DrawTextXY(13, 52, ROMSTR("Connected to CNC"));
	DrawTextXY(1, 36, g_bConnected ? STRING_CHECKED : STRING_UNCHECKED);
	DrawTextXY(1, 52, STRING_UNCHECKED);
	u8g2.drawBox(0, 7, 128, 14);
	u8g2.setColorIndex(0);
	DrawTextXY(1, 10, g_RomSettings.pendantName);
/*	DrawText(0,0,"ABCDEFGHIJKLM");
	DrawText(0,1,"NOPQRSTUVWXYZ");
	DrawText(0,2,"abcdefghijklm");
	DrawText(0,3,"nopqrstuvwxyz");
	DrawText(0,4,"AaBbCcDdEeFfGgHhIi");*/
}

void WelcomeScreen::Update( unsigned long time )
{
	if (g_bConnected && g_MachineStatus != STATUS_DISCONNECTED)
	{
		CloseScreen();
	}
}
