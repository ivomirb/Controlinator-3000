#include "pch.h"
#include "Serial.h"

SerialEmulator Serial;

DWORD WINAPI SerialEmulator::ComThreadProc( void *param )
{
	((SerialEmulator*)param)->ComThread();
	return 0;
}

void SerialEmulator::ComThread( void )
{
	while (1)
	{
		char ch = 0;
		ResetEvent(m_OvRead.hEvent);
		if (!ReadFile(m_ComPort, &ch, 1, nullptr, &m_OvRead))
		{
			if (GetLastError() != ERROR_IO_PENDING || WaitForSingleObject(m_OvRead.hEvent, INFINITE) != WAIT_OBJECT_0)
			{
				continue;
			}
		}
		EnterCriticalSection(&m_InputLock);
		Assert(m_InputQueue.size() < 64);
		m_InputQueue.push_back(ch);
		LeaveCriticalSection(&m_InputLock);
	}
}

int16_t SerialEmulator::available( void )
{
	EnterCriticalSection(&m_InputLock);
	int16_t res = (int16_t)m_InputQueue.size();
	LeaveCriticalSection(&m_InputLock);
	return res;
}

char SerialEmulator::read( void )
{
	EnterCriticalSection(&m_InputLock);
	Assert(!m_InputQueue.empty());
	char res = *m_InputQueue.begin();
	m_InputQueue.erase(m_InputQueue.begin());
	LeaveCriticalSection(&m_InputLock);
	return res;
}


void SerialEmulator::Init(  HWND output )
{
	m_Output = output;
	SendMessage(m_Output, EM_SETLIMITTEXT, 65536, 0);
	InitializeCriticalSection(&m_InputLock);

	m_ComPort = CreateFile("\\\\.\\COM14", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if (m_ComPort != INVALID_HANDLE_VALUE)
	{
		DCB dcb = {sizeof(dcb)};
		GetCommState(m_ComPort, &dcb);
		dcb.BaudRate = CBR_19200;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		SetCommState(m_ComPort, &dcb);

		m_OvRead.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		m_OvWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		CreateThread(nullptr, 0, ComThreadProc, this, 0, nullptr);
	}
}

void SerialEmulator::Print( const char *c )
{
	if (!WriteFile(m_ComPort, c, (int)strlen(c), nullptr, &m_OvWrite))
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			WaitForSingleObject(m_OvWrite.hEvent, 500);
		}
	}
	if (strcmp(c, "\x1F") == 0 || strcmp(c, "PING") == 0 || strcmp(c, "JOY:") == 0 || strcmp(c, "RAWJOY:") == 0 || strcmp(c, "JOG:") == 0)
	{
		m_bSpam = true;
	}
	else if (m_bSpam && strchr(c, '\n'))
	{
		m_bSpam = false;
		return;
	}
	if (!m_bSpam)
	{
		OutputConsole(c);
	}
}

void SerialEmulator::OutputConsole( const char *c )
{
	char buf[1024];
	char *ptr = buf;
	while (*c)
	{
		if (*c == '\n')
		{
			*ptr++ = '\r';
		}
		*ptr++ = *c++;
	}
	*ptr = 0;

	LPARAM lineCount = SendMessage(m_Output, EM_GETLINECOUNT, 0, 0);
	if (lineCount > 1000)
	{
		LRESULT next = SendMessage(m_Output, EM_LINEINDEX, lineCount - 1000, 0);
		SendMessage(m_Output, EM_SETSEL, 0, next);
		SendMessage(m_Output, EM_REPLACESEL, 0, (LPARAM)ptr);
	}
	LRESULT end = GetWindowTextLength(m_Output);
	SendMessage(m_Output, EM_SETSEL, end, end);
	SendMessage(m_Output, EM_REPLACESEL, 0, (LPARAM)buf);
	LRESULT first = SendMessage(m_Output, EM_GETFIRSTVISIBLELINE, 0, 0);
	lineCount = SendMessage(m_Output, EM_GETLINECOUNT, 0, 0);
	if (lineCount > first + 20)
	{
		SendMessage(m_Output, EM_LINESCROLL, 0, lineCount - first - 20);
	}
}
