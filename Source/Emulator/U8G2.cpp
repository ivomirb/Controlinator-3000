#include "pch.h"
#include "U8G2.h"

char g_Screen[64][128];
char g_ScreenCopy[64][128];

void u8g2_t::drawXBMP( int x, int y, int w, int h, const BYTE *bitmap )
{
	int stride = (w+7)/8;
	for (int yy = 0; yy < h; yy++, bitmap += stride)
	{
		if (yy + y < 0 || yy + y > 63) continue;
		for (int xx = 0; xx < w; xx++)
		{
			if (xx + x < 0 || xx + x > 127) continue;
			char pix = (bitmap[xx/8] & (1 << (xx%8))) != 0;
			if (!m_bTransparent || pix)
			{
				g_Screen[yy+y][xx+x] = pix ^ m_ColorIndex;
			}
		}
	}
}

void u8g2_t::drawBox( int x, int y, int w, int h )
{
	char color = !m_ColorIndex;
	for (int yy = 0; yy < h; yy++)
	{
		if (yy +y < 0 || yy +y > 63) continue;
		for (int xx = 0; xx < w; xx++)
		{
			if (xx + x < 0 || xx + x > 127) continue;
			g_Screen[yy+y][xx+x] = color;
		}
	}
}

void u8g2_t::sendBuffer( void )
{
	memcpy(g_ScreenCopy, g_Screen, sizeof(g_Screen));
}

#if U8G2_FULL_BUFFER
void u8g2_t::updateDisplayArea( int tx, int ty, int tw, int th )
{
	for (int y = ty * 8; y < (ty + th) * 8; y++)
	{
		memcpy(g_ScreenCopy[y] + tx * 8, g_Screen[y] + tx * 8, tw * 8);
	}
}
#endif
