#pragma once

#if U8G2_FULL_BUFFER
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1309_128X64_NONAME0_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

// Y position for each row of text. Leaves a larger gap between first and second row for the title
const int8_t g_Rows[5] = {0, 16, 29, 42, 55};

void DrawTextInt( int8_t x, int8_t y, const char *text, bool bBold, bool bFlash )
{
	for (;; text++, x+= 7)
	{
#ifdef EMULATOR
		char c = *text;
#else
		char c = bFlash ? pgm_read_byte(text) : *text;
#endif
		if (c == 0) break;
		uint8_t idx = c;
		if (idx < sizeof(g_Font)/8)
		{
			if (bBold)
			{
				if (c >= '0' && c <= '9') idx -= 32;
			}
			u8g2.drawXBMP(x, (c >= 'a' && c <= 'z') ? y + 1 : y, 7, 8, g_Font +idx*8);
		}
	}
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
