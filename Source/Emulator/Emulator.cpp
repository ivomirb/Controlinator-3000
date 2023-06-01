// This emulator allows you to test the Arduino code on PC and use Visual Studio for development and debugging.
// It creates a GUI representation of the pendant's screen and buttons.
//
// In my testing I used software called "HHD Virtual Serial Port Tools" to create a pair of linked virtual
// USB ports COM13 and COM14. The emulator is hard-coded to use COM14 in Serial.cpp. The OpenBuilds software
// is then able to locate the pendant on COM13 as if it was the real device.
// There is another free software called "com0com", which is supposed to also create virtual USB ports, but
// I was unable to get it to work.
//
// For even more emulation goodness, load the Grbl software onto an Arduino board and hook it to your dev PC.
// The emulation is not perfect, but is close enough for the majority of the dev needs. Final testing needs
// to be done on a real pendant device with a real CNC.

#include "pch.h"
#include "framework.h"
#include "Emulator.h"
#include "U8G2.h"
#include "Serial.h"
#include "EEPROM.h"
#include <math.h>
#include <stdio.h>
#include <commctrl.h>

#pragma warning(disable: 4244)

#define EMULATOR
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
char szTitle[MAX_LOADSTRING];                  // The title bar text
char szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND g_ConsoleDlg;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const int BMP_SCALE = 2;
const int SCREEN_X = 100;
const int SCREEN_Y = 100;

const int BUTTON_SIZE = 40;
const int BUTTON_PADDING_X = 20;
const int BUTTON_PADDING_Y = 10;
const int BUTTON_X1 = SCREEN_X - BUTTON_PADDING_X - BUTTON_SIZE;
const int BUTTON_X2 = SCREEN_X + 128*BMP_SCALE + BUTTON_PADDING_X;
const int BUTTON_Y = SCREEN_X + 32*BMP_SCALE - 2*BUTTON_SIZE - 3*BUTTON_PADDING_Y/2;

const POINT g_ButtonPos[8] =
{
	{BUTTON_X1, BUTTON_Y},
	{BUTTON_X1, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y)},
	{BUTTON_X1, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y) * 2},
	{BUTTON_X1, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y) * 3},
	{BUTTON_X2, BUTTON_Y},
	{BUTTON_X2, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y)},
	{BUTTON_X2, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y) * 2},
	{BUTTON_X2, BUTTON_Y + (BUTTON_SIZE+BUTTON_PADDING_Y) * 3},
};

const int WHEEL_SIZE = 40;
const int WHEEL_X1 = SCREEN_X + 64*BMP_SCALE - BUTTON_PADDING_X/2 - WHEEL_SIZE;
const int WHEEL_X2 = WHEEL_X1 + WHEEL_SIZE + BUTTON_PADDING_X;
const int WHEEL_Y = SCREEN_Y + 64*BMP_SCALE + 50;

const int JOYPAD_SIZE = 200;
const int JOYPAD_X = SCREEN_X + 64*BMP_SCALE - JOYPAD_SIZE/2;
const int JOYPAD_Y = WHEEL_Y + WHEEL_SIZE + BUTTON_PADDING_Y;
const int JOYPAD_PAD = 5;
const int JOYPAD_MARKER_SIZE = 9;

HBITMAP g_ScreenBmp;
DWORD *g_ScreenBits;

HPEN g_ButtonPen;
HBRUSH g_ButtonBrushUp, g_ButtonBrushDown;

int g_MouseCapture;
int g_Ticks;
int16_t g_JoyCenterX = 512;
int16_t g_JoyCenterY = 512;

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;

void dtostrf( float val, int len, int precision, char *buf )
{
	sprintf_s(buf, 20, "%*.*f", len, precision, val);
}

unsigned long millis( void )
{
	return GetTickCount();
}

void InitializeInput( void )
{
}

#define BUTTON_COUNT 12
#define BUTTON_WHEEL_L 10
#define BUTTON_WHEEL_R 11
uint16_t g_PhysicalButtons;

#define Sprintf sprintf_s
#define strcpy_P strcpy_s
#define Strlen (int16_t)strlen
#define strlen_P Strlen
#define ROMSTR(x) x
#define DEFINE_STRING(name, str) const char *name = str;
#define PROGMEM
#define U8G2_FULL_BUFFER 1

#define pinMode(X, Y)

#include "..\Pendant\Main.h"

#ifdef ENABLE_ABORT_BUTTON
const int ABORT_BUTTON_SIZE = 50;
const int ABORT_BUTTON_X = SCREEN_X + 64*BMP_SCALE - ABORT_BUTTON_SIZE/2;
const int ABORT_BUTTON_Y = SCREEN_Y - ABORT_BUTTON_SIZE - BUTTON_PADDING_Y;
#endif

// Reads font.bmp and generates font.txt
void GenerateFont( void )
{
	HBITMAP font = (HBITMAP)LoadImage(NULL, "Pendant\\font.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	BITMAP info;
	GetObject(font, sizeof(info), &info);

	const char *specialNames[32] =
	{
		"unused",
		"unchecked",
		"checked",
		"hold",
		"placeholder",
		"unused",
		"unused",
		"unused",
		"unused",
		"unused",
		"new line",
		"unused",
		"unused",
		"new line",
		"unused",
		"unused",

		"0 bold",
		"1 bold",
		"2 bold",
		"3 bold",
		"4 bold",
		"5 bold",
		"6 bold",
		"7 bold",
		"8 bold",
		"9 bold",
		"% bold",
		"F bold",
		"S bold",
		"X bold",
		"Y bold",
		"Z bold",
	};

	FILE *f;
	fopen_s(&f, "Pendant\\font.txt", "wt");
	for (int i = 0; i < 128; i++)
	{
		int x0 = (i%16) * 8;
		int y0 = 71 - (i/16) * 9;
		const DWORD *pixels = (DWORD*)info.bmBits + y0 * 128 + x0;
		fprintf(f, "\t");
		for (int y = 0; y < 8; y++, pixels -= 128)
		{
			BYTE b = 0;
			for (int x = 0; x < 7; x++)
			{
				b = (b<<1) | ((pixels[6-x]&0xFFFFFF) == 0 ? 1 : 0);
			}
			fprintf(f, "0x%02X, ", b);
		}
		char c = i;
		if (i < 32)
		{
			fprintf(f, "// %s\n", specialNames[i]);
		}
		else if (c == ' ' || c == '\\')
		{
			fprintf(f, "// '%c'\n", c);
		}
		else
		{
			fprintf(f, "// %c\n", c);
		}
	}
	fclose(f);
}

INT_PTR CALLBACK ConsoleProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	if (message == WM_COMMAND && wParam == IDC_BUTTONCLEAR)
	{
		SetDlgItemText(hDlg, IDC_EDITOUTPUT, "");
		return TRUE;
	}
	return FALSE;
}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PENDANTEMULATOR, szWindowClass, MAX_LOADSTRING);

	WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PENDANTEMULATOR));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_PENDANTEMULATOR);
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	RegisterClassEx(&wcex);

	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
	{
		return 1;
	}

	g_ConsoleDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_CONSOLE), hWnd, ConsoleProc);
	SetWindowPos(g_ConsoleDlg, NULL, 500, 0, 0, 0, SWP_NOSIZE);
	Serial.Init(GetDlgItem(g_ConsoleDlg, IDC_EDITOUTPUT));

	setup();
	HandleHandshake();

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	SetTimer(hWnd, 1, 10, nullptr);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PENDANTEMULATOR));

	HDC hdc = GetDC(nullptr);
	BITMAPINFO info = {{sizeof(BITMAPINFOHEADER)}};
	info.bmiHeader.biWidth = 128*BMP_SCALE;
	info.bmiHeader.biHeight = -64*BMP_SCALE;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	g_ScreenBmp = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void**)&g_ScreenBits, NULL, 0);
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 128; x++)
			g_Screen[y][x] = (x+y)&1;


	// uncomment this to convert font.bmp to font.txt
	GenerateFont();

	g_ButtonPen = CreatePen(PS_SOLID, 2, 0x000000);
	g_ButtonBrushUp = CreateSolidBrush(0xE0E0E0);
	g_ButtonBrushDown = CreateSolidBrush(0x707070);

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

void OnPaint( HDC hdc )
{
	// draw screen
	{
		HDC hsrc = CreateCompatibleDC(hdc);
		HGDIOBJ bmp0 = SelectObject(hsrc, g_ScreenBmp);
		BitBlt(hdc, SCREEN_X, SCREEN_Y, 128*BMP_SCALE, 64*BMP_SCALE, hsrc, 0, 0, SRCCOPY);
		SelectObject(hsrc, bmp0);
		DeleteDC(hsrc);
	}
	HGDIOBJ pen0 = SelectObject(hdc, g_ButtonPen);
	HGDIOBJ brush0 = SelectObject(hdc, GetStockObject(NULL_BRUSH));
	Rectangle(hdc, SCREEN_X - 1, SCREEN_Y - 1, SCREEN_X + 128 * BMP_SCALE + 2, SCREEN_Y + 64 * BMP_SCALE + 2);

	// draw butons
	for (int i = 0; i < 8; i++)
	{
		int x = g_ButtonPos[i].x;
		int y = g_ButtonPos[i].y;
		SelectObject(hdc, TestBit(g_ButtonState, i) ? g_ButtonBrushUp : g_ButtonBrushDown);
		Ellipse(hdc, x, y, x + BUTTON_SIZE, y + BUTTON_SIZE);
	}

#ifdef ENABLE_ABORT_BUTTON
	// draw abort button
	SelectObject(hdc, TestBit(g_ButtonState, BUTTON_ABORT) ? g_ButtonBrushUp : g_ButtonBrushDown);
	Ellipse(hdc, ABORT_BUTTON_X, ABORT_BUTTON_Y, ABORT_BUTTON_X + ABORT_BUTTON_SIZE, ABORT_BUTTON_Y + ABORT_BUTTON_SIZE);
#endif

	// draw wheel arrows
	SelectObject(hdc, TestBit(g_ButtonState, BUTTON_WHEEL_L) ? g_ButtonBrushUp : g_ButtonBrushDown);
	Rectangle(hdc, WHEEL_X1, WHEEL_Y, WHEEL_X1 + WHEEL_SIZE, WHEEL_Y + WHEEL_SIZE);
	SelectObject(hdc, TestBit(g_ButtonState, BUTTON_WHEEL_R) ? g_ButtonBrushUp : g_ButtonBrushDown);
	Rectangle(hdc, WHEEL_X2, WHEEL_Y, WHEEL_X2 + WHEEL_SIZE, WHEEL_Y + WHEEL_SIZE);
	{
		int x = WHEEL_X1 + WHEEL_SIZE/2 + 2;
		int y = WHEEL_Y + WHEEL_SIZE/2;
		MoveToEx(hdc, x, y - 6, NULL);
		LineTo(hdc, x - 6, y);
		LineTo(hdc, x, y + 6);
		x = WHEEL_X2 + WHEEL_SIZE/2 - 3;
		MoveToEx(hdc, x, y - 6, NULL);
		LineTo(hdc, x + 6, y);
		LineTo(hdc, x, y + 6);
	}

	// draw joypad
	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SelectObject(hdc, GetStockObject(WHITE_BRUSH));
	Rectangle(hdc, JOYPAD_X, JOYPAD_Y, JOYPAD_X + JOYPAD_SIZE, JOYPAD_Y + JOYPAD_SIZE);
	{
		int size = JOYPAD_SIZE - JOYPAD_PAD*2;
		int x = (g_JoyX * size) / 1024;
		int y = ((1023 - g_JoyY) * size) / 1024;
		x += JOYPAD_X + JOYPAD_PAD;
		y += JOYPAD_Y + JOYPAD_PAD;
		MoveToEx(hdc, x, y - JOYPAD_MARKER_SIZE/2, NULL);
		LineTo(hdc, x, y - JOYPAD_MARKER_SIZE/2 + JOYPAD_MARKER_SIZE);
		MoveToEx(hdc, x - JOYPAD_MARKER_SIZE/2, y, NULL);
		LineTo(hdc, x - JOYPAD_MARKER_SIZE/2 + JOYPAD_MARKER_SIZE, y);
	}

	SelectObject(hdc, pen0);
	SelectObject(hdc, brush0);
}

void UpdateState( int dt )
{
	if (g_MouseCapture == 0)
	{
		g_PhysicalButtons = 0;
	}
	else if (g_MouseCapture != BUTTON_JOYSTICK + 1)
	{
		g_PhysicalButtons = 1 << (g_MouseCapture - 1);
	}

	if (g_MouseCapture != BUTTON_JOYSTICK + 1)
	{
		// auto-center stick
		if (g_JoyX != g_JoyCenterX || g_JoyY != g_JoyCenterY)
		{
			double speed = dt * 3;
			double dx = g_JoyX - g_JoyCenterX;
			double dy = g_JoyY - g_JoyCenterY;
			double dist = sqrt(dx*dx + dy*dy);
			if (dist < speed)
			{
				g_JoyX = g_JoyCenterX;
				g_JoyY = g_JoyCenterY;
			}
			else
			{
				dx *= speed/dist;
				dy *= speed/dist;
				g_JoyX = (int)(g_JoyX - dx);
				g_JoyY = (int)(g_JoyY - dy);
			}
		}
	}

	if (GetKeyState(VK_MBUTTON) < 0)
	{
		g_PhysicalButtons |= 1 << BUTTON_JOYSTICK;
	}
	else
	{
		g_PhysicalButtons &= ~(1 << BUTTON_JOYSTICK);
	}
}

void UpdateScreenBmp( void )
{
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 128; x++)
		{
			DWORD pix = g_Screen[y][x] ? 0xFFFFFF : 0x000000;
			for (int yy = 0; yy < BMP_SCALE; yy++)
			{
				DWORD *bits = g_ScreenBits + ((y*BMP_SCALE+yy)*128 + x) * BMP_SCALE;
				for (int xx = 0; xx < BMP_SCALE; xx++)
				{
					bits[xx] = pix;
				}
			}
		}
}

POINT TransformJoypad( int mx, int my )
{
	int size = JOYPAD_SIZE - JOYPAD_PAD*2;
	mx -= JOYPAD_X + JOYPAD_PAD;
	if (mx < 0) mx = 0;
	if (mx >= size) mx = size - 1;
	my -= JOYPAD_Y + JOYPAD_PAD;
	if (my < 0) my = 0;
	if (my >= size) my = size - 1;
	POINT p;
	p.x = mx * 1024 / size;
	p.y = my * 1024 / size;
	return p;
}

void OnLButtonDown( HWND hWnd, int mx, int my )
{
	if (g_MouseCapture)
	{
		return;
	}

	for (int i = 0; i < 8; i++)
	{
		int x = g_ButtonPos[i].x;
		int y = g_ButtonPos[i].y;
		if (mx >= x && mx < x + BUTTON_SIZE && my >= y && my < y + BUTTON_SIZE)
		{
			g_MouseCapture = i + 1;
			SetCapture(hWnd);
			return;
		}
	}

#ifdef ENABLE_ABORT_BUTTON
	if (mx >= ABORT_BUTTON_X && mx < ABORT_BUTTON_X + ABORT_BUTTON_SIZE && my >= ABORT_BUTTON_Y && my < ABORT_BUTTON_Y + ABORT_BUTTON_SIZE)
	{
		g_MouseCapture = BUTTON_ABORT + 1;
		SetCapture(hWnd);
		return;
	}
#endif

	if (my >= WHEEL_Y && my < WHEEL_Y + WHEEL_SIZE)
	{
		if (mx >= WHEEL_X1 && mx < WHEEL_X1 + WHEEL_SIZE)
		{
			g_MouseCapture = BUTTON_WHEEL_L + 1;
			EncoderAddValue(-1);
			SetCapture(hWnd);
		}
		else if (mx >= WHEEL_X2 && mx < WHEEL_X2 + WHEEL_SIZE)
		{
			g_MouseCapture = BUTTON_WHEEL_R + 1;
			EncoderAddValue(1);
			SetCapture(hWnd);
		}
		return;
	}

	if (mx >= JOYPAD_X && mx < JOYPAD_X + JOYPAD_SIZE && my >= JOYPAD_Y && my < JOYPAD_Y + JOYPAD_SIZE)
	{
		g_MouseCapture = BUTTON_JOYSTICK + 1;
		SetCapture(hWnd);
		POINT p = TransformJoypad(mx, my);
		g_JoyX = (short)p.x;
		g_JoyY = 1023 - (short)p.y;
	}
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			OnPaint(hdc);
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_LBUTTONDOWN:
		{
			int mx = (short)LOWORD(lParam);
			int my = (short)HIWORD(lParam);
			OnLButtonDown(hWnd, mx, my);
		}
		break;

	case WM_LBUTTONUP:
		if (g_MouseCapture)
		{
			ReleaseCapture();
			if (g_MouseCapture == BUTTON_JOYSTICK + 1)
			{
				// 490 - 540
				g_JoyCenterX = 490 + rand()*50/RAND_MAX;
				g_JoyCenterY = 490 + rand()*50/RAND_MAX;
			}
			g_MouseCapture = 0;
		}
		break;

	case WM_MOUSEMOVE:
		if (g_MouseCapture == BUTTON_JOYSTICK + 1)
		{
			int mx = (short)LOWORD(lParam);
			int my = (short)HIWORD(lParam);
			POINT p = TransformJoypad(mx, my);
			g_JoyX = (short)p.x;
			g_JoyY = 1023 - (short)p.y;
		}
		break;

	case WM_TIMER:
		if (wParam == 1)
		{
			int t = GetTickCount();
			int dt = g_Ticks ? t - g_Ticks : 0;
			g_Ticks = t;
			UpdateState(dt);
			loop();
			UpdateScreenBmp();
			InvalidateRect(hWnd, NULL, FALSE);
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
