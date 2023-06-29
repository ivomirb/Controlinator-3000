#pragma once

u8g2_t u8g2;

void InitializeGraphics( void )
{
#ifndef EMULATOR
#if U8G2_FULL_BUFFER
	u8g2_Setup_ssd1309_i2c_128x64_noname0_f(&u8g2, U8G2_R0, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
#else
	u8g2_Setup_ssd1309_i2c_128x64_noname0_2(&u8g2, U8G2_R0, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
#endif
	u8x8_SetPin_HW_I2C(u8g2_GetU8x8(&u8g2), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
	u8g2_InitDisplay(&u8g2);
	u8g2_ClearDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
#endif
}

// Y position for each row of text. Leaves a larger gap between first and second row for the title
const int8_t g_Rows[5] = {0, 16, 29, 42, 55};

#if PARTIAL_SCREEN_UPDATE
uint8_t g_DirtyRect[4]; // x1, y1, x2, y2 (in pixels)

void ClearDirtyRect( void )
{
	g_DirtyRect[0] = 128;
	g_DirtyRect[1] = 64;
	g_DirtyRect[2] = g_DirtyRect[3] = 0;
}

void UpdateDirtyRect( void )
{
	if (g_DirtyRect[0] == 0 && g_DirtyRect[1] == 0 && g_DirtyRect[2] == 128 && g_DirtyRect[3] == 64)
	{
		u8g2_SendBuffer(&u8g2);
	}
	else if (g_DirtyRect[0] < g_DirtyRect[2])
	{
		int8_t tx1 = g_DirtyRect[0] / 8;
		int8_t ty1 = g_DirtyRect[1] / 8;
		int8_t tx2 = (g_DirtyRect[2] + 7) / 8;
		int8_t ty2 = (g_DirtyRect[3] + 7) / 8;
		u8g2_UpdateDisplayArea(&u8g2, tx1, ty1, tx2 - tx1, ty2 - ty1);
	}
	ClearDirtyRect();
}

void InvalidateRect( int16_t x, int16_t y, int16_t w, int16_t h )
{
	int16_t x2 = x + w;
	int16_t y2 = y + h;
	if (x < 0) x = 0;
	if (x2 > 128) x2 = 128;
	if (y < 0) y = 0;
	if (y2 > 64) y2 = 64;
	if (x < x2 && y < y2)
	{
		if (g_DirtyRect[0] > x) g_DirtyRect[0] = x;
		if (g_DirtyRect[1] > y) g_DirtyRect[1] = y;
		if (g_DirtyRect[2] < x2) g_DirtyRect[2] = x2;
		if (g_DirtyRect[3] < y2) g_DirtyRect[3] = y2;
	}
}

void InvalidateRect( void )
{
	g_DirtyRect[0] = g_DirtyRect[1] = 0;
	g_DirtyRect[2] = 128;
	g_DirtyRect[3] = 64;
}

#endif

#if U8G2_FULL_BUFFER
void ClearBuffer( void )
{
	u8g2_ClearBuffer(&u8g2);
#if PARTIAL_SCREEN_UPDATE
	InvalidateRect();
#endif
}
#endif

void SetDrawColor( uint8_t color )
{
	u8g2_SetDrawColor(&u8g2, color);
}

void DrawBox( uint8_t x, uint8_t y, uint8_t w, uint8_t h )
{
	u8g2_DrawBox(&u8g2, x, y, w, h);
#if PARTIAL_SCREEN_UPDATE
	InvalidateRect(x, y, w, h);
#endif
}

void DrawTextInt( int8_t x, int8_t y, const char *text, bool bBold, bool bRomStr )
{
	int8_t x0 = x;
	for (;; text++, x+= 7)
	{
#ifdef EMULATOR
		char c = *text;
#else
		char c = bRomStr ? pgm_read_byte(text) : *text;
#endif
		if (c == 0) break;
		uint8_t idx = c;
		if (idx < sizeof(g_Font)/8)
		{
			if (bBold)
			{
				if (c >= '0' && c <= '9') idx -= 32;
			}
			u8g2_DrawXBMP(&u8g2, x, (c >= 'a' && c <= 'z') ? y + 1 : y, 7, 8, g_Font +idx*8);
		}
	}

#if PARTIAL_SCREEN_UPDATE
	InvalidateRect(x0, y, x - x0, 8);
#endif
}

void DrawTextXY( int8_t x, int8_t y, const char *text )
{
	DrawTextInt(x, y, text, false, false);
}

void DrawText( int8_t col, int8_t row, const char *text )
{
	DrawTextInt(col * 7 + 1, g_Rows[row], text, false, false);
}

void DrawTextBoldXY( int8_t x, int8_t y, const char *text )
{
	DrawTextInt(x, y, text, true, false);
}

void DrawTextBold( int8_t col, int8_t row, const char *text )
{
	DrawTextInt(col * 7 + 1, g_Rows[row], text, true, false);
}


#ifndef EMULATOR
void DrawTextXY( int8_t x, int8_t y, const __FlashStringHelper *text )
{
	DrawTextInt(x, y, (const char*)text, false, true);
}

void DrawText( int8_t col, int8_t row, const __FlashStringHelper *text )
{
	DrawTextInt(col * 7 + 1, g_Rows[row], (const char*)text, false, true);
}

void DrawTextBoldXY( int8_t x, int8_t y, const __FlashStringHelper *text )
{
	DrawTextInt(x, y, (const char*)text, true, true);
}

void DrawTextBold( int8_t col, int8_t row, const __FlashStringHelper *text )
{
	DrawTextInt(col * 7 + 1, g_Rows[row], (const char*)text, true, true);
}
#endif
