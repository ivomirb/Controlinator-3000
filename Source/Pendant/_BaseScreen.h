#pragma once

#define DRAW_SCREEN_TITLE 1 // 1 - draw title, 0 - no title (saves memory)

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

#if U8G2_FULL_BUFFER
	static void ClearScreen( void );
#if PARTIAL_SCREEN_UPDATE
	static void UpdateScreen( void );
#endif
#endif

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
#if DRAW_SCREEN_TITLE
#ifdef EMULATOR
	static void DrawMachineStatus( const char *title, uint8_t titleLen );
#else
	static void DrawMachineStatus( const __FlashStringHelper *title, uint8_t titleLen );
#endif
#else
	#define DrawMachineStatus(...) DrawMachineStatusInt()
	static void DrawMachineStatusInt( void );
#endif

	// Draws a blank button symbol for unused buttons according to the mask
	static void DrawUnusedButtons( uint16_t mask );

	// Draws a button on the screen. if bHold is true, draws the hold icon and highlights the label if the button is pressed
	static void DrawButton( uint8_t button, const char *label, uint8_t labelLen, bool bHold );
#ifndef EMULATOR
	static void DrawButton( uint8_t button, const __FlashStringHelper *label, uint8_t labelLen, bool bHold );
#endif

	// Closes the screen if it is active and switches to the default main screen
	void CloseScreen( void );

#if PARTIAL_SCREEN_UPDATE
	struct DrawStateBase
	{
		uint16_t buttonState;
		uint16_t buttonHold;
		uint16_t buttonDown;
		MachineStatus machineStatus;
		int8_t jobProgress;
		bool bDrawAll;

		uint8_t custom[32]; // for use by the current screen, initialized to 0xFF
	};
#else
	struct DrawStateBase
	{
		const bool bDrawAll = true;
	};
#endif

	static DrawStateBase s_DrawState;

private:
	static void DrawButtonInt( uint8_t button, const char *label, uint8_t labelLen, bool bHold, bool bRomStr );
};

///////////////////////////////////////////////////////////////////////////////

BaseScreen::DrawStateBase BaseScreen::s_DrawState;

void BaseScreen::Activate( unsigned long time )
{
	if (s_pCurrentScreen && s_pCurrentScreen != this)
	{
		s_pCurrentScreen->Deactivate();
	}
	if (s_pCurrentScreen != this)
	{
		ReleaseAllButtons();

#if PARTIAL_SCREEN_UPDATE
		// clear draw state on screen change
		memset(&s_DrawState, 0xFF, sizeof(s_DrawState));
		g_DirtyRect[0] = g_DirtyRect[1] = 0;
		g_DirtyRect[2] = 128;
		g_DirtyRect[3] = 64;
#endif
	}
	s_pCurrentScreen = this;
}

#if U8G2_FULL_BUFFER
void BaseScreen::ClearScreen( void )
{
#if PARTIAL_SCREEN_UPDATE
	s_DrawState.bDrawAll |= s_DrawState.buttonState != g_ButtonState ||
		s_DrawState.buttonHold != g_ButtonHold ||
		s_DrawState.buttonDown != g_ButtonDown ||
		s_DrawState.machineStatus != g_MachineStatus ||
		s_DrawState.jobProgress != g_JobProgress;
	if (!s_DrawState.bDrawAll)
	{
		return;
	}
#endif
	ClearBuffer();
}

#if PARTIAL_SCREEN_UPDATE
void BaseScreen::UpdateScreen( void )
{
	UpdateDirtyRect();
	s_DrawState.buttonState = g_ButtonState;
	s_DrawState.buttonHold = g_ButtonHold;
	s_DrawState.buttonDown = g_ButtonDown;
	s_DrawState.machineStatus = g_MachineStatus;
	s_DrawState.jobProgress = g_JobProgress;
	s_DrawState.bDrawAll = false;
}
#endif
#endif

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

#if DRAW_SCREEN_TITLE
#ifdef EMULATOR
void BaseScreen::DrawMachineStatus( const char *title, uint8_t titleLen )
{
	Assert((title ? Strlen(title) : 0) == titleLen);
#else
void BaseScreen::DrawMachineStatus( const __FlashStringHelper *title, uint8_t titleLen )
{
#endif
#else
void BaseScreen::DrawMachineStatusInt( void )
{
	const char *title = nullptr;
	const uint8_t titleLen = 0;
#endif

#if DRAW_SCREEN_TITLE
	if (titleLen > 0)
	{
		uint8_t x = (50 - (titleLen * 7)) / 2;
		DrawBox(x - 2, 0, titleLen * 7 + 4, 10);
		SetDrawColor(0);
		DrawTextXY(x, 1, title);
		SetDrawColor(1);
	}
#endif
	{
		int8_t len = GetStatusNameLen(g_MachineStatus);
		if (g_MachineStatus == STATUS_RUN && g_bJobRunning && g_JobProgress != -1)
		{
			len += 5;
		}
#if DRAW_SCREEN_TITLE
		uint8_t x = 55;
#else
		uint8_t x = 56 - len * 7 / 2;
#endif
		DrawTextXY(x, 0, ROMSTR("["));
		DrawTextXY(x + 7, 0, GetStatusName(g_MachineStatus));
		if (g_MachineStatus == STATUS_RUN && g_bJobRunning && g_JobProgress != -1)
		{
			Sprintf(g_TextBuf, " %3d%%", g_JobProgress);
			DrawTextXY(x + 28, 0, g_TextBuf);
		}
		DrawTextXY(x + len * 7 + 7, 0, ROMSTR("]"));
		DrawBox(0, 10, 128, 1);
	}
}

void BaseScreen::DrawUnusedButtons( uint16_t mask )
{
	SetDrawColor(1);
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

void BaseScreen::DrawButtonInt( uint8_t button, const char *label, uint8_t labelLen, bool bHold, bool bRomStr )
{
	Assert(Strlen(label) == labelLen);
	SetDrawColor(1);
	if (button < 4)
	{
		// left side
		int8_t x = 0;
		int8_t y = g_Rows[button + 1];
		if (bHold)
		{
			if (TestBit(g_ButtonDown, button))
			{
				DrawBox(0, y - 1, labelLen*7 + 9, 10);
				SetDrawColor(0);
			}
			DrawTextXY(0, y, g_StrHold);
			x++;
		}
		DrawTextInt(x*7 + 1, y, label, false, bRomStr);
	}
	else
	{
		// right side
		int8_t x = 17 - labelLen;
		int8_t y = g_Rows[button - 3];
		if (bHold)
		{
			if (TestBit(g_ButtonDown, button))
			{
				DrawBox(x*7, y - 1, labelLen*7 + 9, 10);
				SetDrawColor(0);
			}
			DrawTextXY(17*7 + 2, y, g_StrHold);
			x--;
		}
		DrawTextInt(x*7 + 8, y, label, false, bRomStr);
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
