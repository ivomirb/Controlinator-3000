#pragma once

#include <stdio.h>
#include <string>

class SerialEmulator
{
public:
	void Init( HWND output );

	void begin( int ) {}
	void print( int i ) { char buf[100]; sprintf_s(buf, "%d", i); Print(buf); }
	void print( unsigned int u ) { char buf[100]; sprintf_s(buf, "%u", u); Print(buf); } 
	void print( float f ) { char buf[100]; sprintf_s(buf, "%.3f", f); Print(buf); }
	void print( const char *c ) { Print(c); }

	void println( int i ) { print(i); Print("\r\n"); }
	void println( unsigned int u ) { print(u); Print("\r\n"); }
	void println( float f ) { print(f); Print("\r\n"); }
	void println( const char *c ) { print(c); Print("\r\n"); }

	int16_t available( void );
	char read( void );

	void OutputConsole( const char *c );

private:
	void Print( const char *c );

	HWND m_Output;
	HANDLE m_ComPort;
	OVERLAPPED m_OvRead;
	OVERLAPPED m_OvWrite;

	CRITICAL_SECTION m_InputLock;
	std::string m_InputQueue;
	bool m_bSpam;

	static DWORD WINAPI ComThreadProc( void *param );

	void ComThread( void );
};

extern SerialEmulator Serial;
