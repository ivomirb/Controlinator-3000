#pragma once

extern char g_Screen[64][128];

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
	void sendBuffer( void ) {}

private:
	bool m_bTransparent;
	char m_ColorIndex;
};

#define U8G2_SSD1309_128X64_NONAME0_F_HW_I2C U8G2
#define U8X8_PROGMEM
#define U8G2_R0 0
#define U8X8_PIN_NONE 0
