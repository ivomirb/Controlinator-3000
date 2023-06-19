#pragma once

#include "..\Pendant\Config.h" // for U8G2_FULL_BUFFER

extern char g_Screen[64][128];
extern char g_ScreenCopy[64][128];

class U8G2
{
public:
	U8G2( int, int ) { m_bTransparent = false; m_ColorIndex = 0; }
	void begin( void ) {}
	void clearBuffer( void ) { memset(g_Screen, 0, sizeof(g_Screen)); }
	void drawXBMP( int x, int y, int w, int h, const BYTE *bitmap );
	void drawBox( int x, int y, int w, int h );
	void setBitmapMode( char transparent ) { m_bTransparent = (transparent != 0); }
	void setColorIndex( char index ) { m_ColorIndex = !index; }
	void sendBuffer( void );

#if U8G2_FULL_BUFFER
	void updateDisplayArea( int tx, int ty, int tw, int th );
#else
	void firstPage( void ) { clearBuffer(); }
	bool nextPage( void ) { sendBuffer(); return false; }
#endif

private:
	bool m_bTransparent;
	char m_ColorIndex;
};

#if U8G2_FULL_BUFFER
#define U8G2_SSD1309_128X64_NONAME0_F_HW_I2C U8G2
#else
#define U8G2_SSD1309_128X64_NONAME0_2_HW_I2C U8G2
#endif

#define U8X8_PROGMEM
#define U8G2_R0 0
#define U8X8_PIN_NONE 0
