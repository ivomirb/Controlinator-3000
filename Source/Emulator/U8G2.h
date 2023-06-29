#pragma once

#include "..\Pendant\Config.h" // for U8G2_FULL_BUFFER

extern char g_Screen[64][128];
extern char g_ScreenCopy[64][128];

class u8g2_t
{
public:
	u8g2_t( void ) { m_bTransparent = false; m_ColorIndex = 0; }
	void clearBuffer( void ) { memset(g_Screen, 0, sizeof(g_Screen)); }
	void drawXBMP( int x, int y, int w, int h, const BYTE *bitmap );
	void drawBox( int x, int y, int w, int h );
	void setBitmapMode( char transparent ) { m_bTransparent = (transparent != 0); }
	void setDrawColor( char index ) { m_ColorIndex = !index; }
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

inline void u8g2_SendBuffer( u8g2_t *obj ) { obj->sendBuffer(); }
#if U8G2_FULL_BUFFER
inline void u8g2_UpdateDisplayArea( u8g2_t *obj, int tx, int ty, int tw, int th ) { obj->updateDisplayArea(tx, ty, tw, th); }
#else
inline void u8g2_FirstPage( u8g2_t *obj ) { obj->firstPage(); }
inline bool u8g2_NextPage( u8g2_t *obj ) { return obj->nextPage(); }
#endif
inline void u8g2_ClearBuffer( u8g2_t *obj ) { obj->clearBuffer(); }
inline void u8g2_SetDrawColor( u8g2_t *obj, char index ) { obj->setDrawColor(index); }
inline void u8g2_DrawBox( u8g2_t *obj, int x, int y, int w, int h ) { obj->drawBox(x, y, w, h); }
inline void u8g2_DrawXBMP( u8g2_t *obj, int x, int y, int w, int h, const BYTE *bitmap ) { obj->drawXBMP(x, y, w, h, bitmap); }

#define U8X8_PROGMEM
#define U8G2_R0 0
#define U8X8_PIN_NONE 0
