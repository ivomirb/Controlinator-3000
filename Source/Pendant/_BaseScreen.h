#pragma once

// Base class for all screens
class BaseScreen
{
public:
	// Draws the contents of the screen. The buffer is clear and the current color is 1
	virtual void Draw( void ) = 0;

	// Updates the screen every frame. time is the current global time
	virtual void Update( unsigned long time ) = 0;

	// Makes this the active screen
	virtual void Activate( unsigned long time );

	// Called when another screen is activated
	virtual void Deactivate( void ) {}

	bool IsActive( void ) const { return s_pCurrentScreen == this; }

	static BaseScreen *s_pCurrentScreen;

protected:
	static int8_t GetCurrentButton( void );

	// Prints a value to the buffer, using the curren mm/inch setting
	static void PrintCoord( char *buf, float val );

	// Print X/Y/Z to the buffer. Uses the current WCS/MCS and mm/inch settings
	static void PrintX( char *buf );
	static void PrintY( char *buf );
	static void PrintZ( char *buf );

	// Draws the status line at the top of the screen
	static void DrawMachineStatus( void );

	// Draws a blank button symbol for unused buttons according to the mask
	static void DrawUnusedButtons( uint16_t mask );

	// Draws a button on the screen. if bHold is true, draws the hold icon and highlights the label if the button is pressed
	static void DrawButton( uint8_t button, const char *label, uint8_t labelLen, bool bHold );
#ifndef EMULATOR
	static void DrawButton( uint8_t button, const __FlashStringHelper *label, uint8_t labelLen, bool bHold );
#endif

	// Closes the screen if it is active and switches to the default main screen
	void CloseScreen( void );

private:
	static void DrawButtonInt( uint8_t button, const char *label, uint8_t labelLen, bool bHold, bool bFlash );
};

///////////////////////////////////////////////////////////////////////////////

void BaseScreen::Activate( unsigned long time )
{
	if (s_pCurrentScreen && s_pCurrentScreen != this)
	{
		s_pCurrentScreen->Deactivate();
	}
	if (s_pCurrentScreen != this)
	{
		ReleaseAllButtons();
	}
	s_pCurrentScreen = this;
}

int8_t BaseScreen::GetCurrentButton( void )
{
	if (g_ButtonClick)
	{
		for (int8_t i = 0; i < BUTTON_COUNT; i++)
		{
			if (TestBit(g_ButtonClick, i))
			{
				return i;
			}
		}
	}
	return -1;
}

void BaseScreen::PrintCoord( char *buf, float val )
{
	if (g_bShowInches)
	{
		dtostrf(val / 25.4f, 8, 3, buf);
	}
	else
	{
		dtostrf(val, 8, 2, buf);
	}
}

void BaseScreen::PrintX( char *buf )
{
	PrintCoord(buf, g_bWorkSpace ? g_WorkX : (g_WorkX + g_OffsetX));
}

void BaseScreen::PrintY( char *buf )
{
	PrintCoord(buf, g_bWorkSpace ? g_WorkY : (g_WorkY + g_OffsetY));
}

void BaseScreen::PrintZ( char *buf )
{
	PrintCoord(buf, g_bWorkSpace ? g_WorkZ : (g_WorkZ + g_OffsetZ));
}

void BaseScreen::DrawMachineStatus( void )
{
	int8_t len = GetStatusNameLen(g_MachineStatus);
	uint8_t x = (16 - len) / 2;
	DrawText(x, 0, ROMSTR("["));
	DrawText(x + 1, 0, GetStatusName(g_MachineStatus));
	DrawText(x + len + 1, 0, ROMSTR("]"));
}

void BaseScreen::DrawUnusedButtons( uint16_t mask )
{
	u8g2.setColorIndex(1);
	for (uint16_t i = 0; i < 8; i++)
	{
		if (TestBit(mask, i))
		{
			if (i < 4)
			{
				DrawText(0, i + 1, g_StrPlaceholder);
			}
			else
			{
				DrawText(17, i - 3, g_StrPlaceholder);
			}
		}
	}
}

void BaseScreen::DrawButtonInt( uint8_t button, const char *label, uint8_t labelLen, bool bHold, bool bFlash )
{
	Assert(Strlen(label) == labelLen);
	u8g2.setColorIndex(1);
	if (button < 4)
	{
		// left side
		int8_t x = 0;
		int8_t y = g_Rows[button + 1];
		if (bHold)
		{
			if (IsButtonDown(button))
			{
				u8g2.drawBox(0, y - 1, labelLen*7 + 9, 10);
				u8g2.setColorIndex(0);
			}
			DrawTextXY(0, y, g_StrHold);
			x++;
		}
		DrawTextInt(x*7 + 1, y, label, false, bFlash);
	}
	else
	{
		// right side
		int8_t x = 17 - labelLen;
		int8_t y = g_Rows[button - 3];
		if (bHold)
		{
			if (IsButtonDown(button))
			{
				u8g2.drawBox(x*7, y - 1, labelLen*7 + 9, 10);
				u8g2.setColorIndex(0);
			}
			DrawTextXY(17*7 + 2, y, g_StrHold);
			x--;
		}
		DrawTextInt(x*7 + 8, y, label, false, bFlash);
	}
}

void BaseScreen::DrawButton( uint8_t button, const char *label, uint8_t labelLen, bool bHold )
{
	DrawButtonInt(button, label, labelLen, bHold, false);
}

#ifndef EMULATOR
void BaseScreen::DrawButton( uint8_t button, const __FlashStringHelper *label, uint8_t labelLen, bool bHold )
{
	DrawButtonInt(button, (const char*)label, labelLen, bHold, true);
}
#endif
