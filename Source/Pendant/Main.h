#define PENDANT_VERSION "1.0" // must match Pendant.js
#define PENDANT_BAUD_RATE 38400 // must match Pendant.js

#include "Config.h"

const uint8_t g_Font[] U8X8_PROGMEM =
{
#include "font.h"
};

char g_TextBuf[20];

#include "SpecialStrings.h"
#include "RomSettings.h"
#include "Graphics.h"
#include "Input.h"
#include "MachineStatus.h"

const unsigned long PING_TIME = 10000; // 10 seconds of no PONG will disconnect (could be shorter, but I noticed that when VSCode starts up, the COM traffic stalls for a few seconds)
const unsigned long SHOW_STOP_TIME = 500; // after 500ms after the last idle, allow showing s STOP button

///////////////////////////////////////////////////////////////////////////////
// State

bool g_bConnected;
unsigned long g_CurrentTime;
unsigned long g_LastPingTime;
unsigned long g_LastPongTime;
unsigned long g_LastIdleTime;
unsigned long g_LastBusyTime;
bool g_bCanShowStop;

// coordinate systems
float g_WorkX, g_WorkY, g_WorkZ;
float g_OffsetX, g_OffsetY, g_OffsetZ; // machine = work + offset
bool g_bWorkSpace = true;
bool g_bShowInches = true;
bool g_bJobRunning = false;
bool g_bRecentlyHomed = false;

enum
{
	TLO_ENABLED = 1,
	TLO_HAS_REF = 2,
	TLO_IN_POSITION = 4,
};

uint8_t g_TloState = 0;

// feed and speed
uint16_t g_FeedOverride;
uint16_t g_SpeedOverride;
uint16_t g_RealFeed;
uint16_t g_RealSpeed;

///////////////////////////////////////////////////////////////////////////////

#include "_BaseScreen.h"
BaseScreen *BaseScreen::s_pCurrentScreen;
#include "_ScreenClasses.h"

AlarmScreen g_AlarmScreen;
#ifndef DISABLE_CALIBRATION_SCREEN
CalibrationScreen g_CalibrationScreen;
#endif
DialogScreen g_DialogScreen;
JogScreen g_JogScreen;
#ifndef DISABLE_MACRO_SCREEN
MacroScreen g_MacroScreen;
#endif
MainScreen g_MainScreen;
ProbeMenuScreen g_ProbeMenuScreen;
RunScreen g_RunScreen;
WelcomeScreen g_WelcomeScreen;
ZProbeScreen g_ZProbeScreen;

// This is outside of _BaseScreen.h because it needs access to the other screens
void BaseScreen::CloseScreen( void )
{
	if (IsActive())
	{
		if (!g_bConnected || g_MachineStatus == STATUS_DISCONNECTED)
		{
			g_WelcomeScreen.Activate(g_CurrentTime);
		}
		else if (g_MachineStatus == STATUS_ALARM)
		{
			g_AlarmScreen.Activate(g_CurrentTime);
		}
		else
		{
			g_MainScreen.Activate(g_CurrentTime);
		}
	}
}

#include "AlarmScreen.h"
#include "CalibrationScreen.h"
#include "DialogScreen.h"
#include "JogScreen.h"
#include "MacroScreen.h"
#include "MainScreen.h"
#include "ProbeMenuScreen.h"
#include "RunScreen.h"
#include "WelcomeScreen.h"
#include "ZProbeScreen.h"

///////////////////////////////////////////////////////////////////////////////

char g_SerialBuffer[128];
int g_SerialBufferLen = 0;

#define CHAR_ACK '\x1F'
DEFINE_STRING(g_StrAck, "\x1F");

const char *ProcessSerial( void )
{
	int16_t av = Serial.available();
	if (av > 0)
	{
		for (int16_t i = 0; i < av; i++)
		{
			char ch = Serial.read();

			if (ch == CHAR_ACK)
			{
				// incomplete command. notify PC to send more
				Serial.println(g_StrAck);
				return NULL;
			}

			if (ch == '\n')
			{
				g_SerialBuffer[g_SerialBufferLen] = 0;
				g_SerialBufferLen = 0;
				if (strcmp(g_SerialBuffer,"PEN") != 0) // the initial handshake doesn't need ACK
				{
					Serial.println(g_StrAck);
				}
				return g_SerialBuffer;
			}
			else if (g_SerialBufferLen < sizeof(g_SerialBuffer) - 1)
			{
				g_SerialBuffer[g_SerialBufferLen++] = ch;
			}
		}
	}

	return NULL;
}

// Parses the STATUS: string from the PC
// string format: <status>|workX,workY,workZ|feed%,rpm%,realFeed,realRpm
void ParseStatus( const char *status )
{
	g_MachineStatus = (MachineStatus)atol(status); status = strchr(status, '|') + 1;
	g_WorkX = atof(status); status = strchr(status, ',') + 1;
	g_WorkY = atof(status); status = strchr(status, ',') + 1;
	g_WorkZ = atof(status); status = strchr(status, '|') + 1;
	g_FeedOverride = atoi(status); status = strchr(status, ',') + 1;
	g_SpeedOverride = atoi(status); status = strchr(status, ',') + 1;
	g_RealFeed = atoi(status); status = strchr(status, ',') + 1;
	g_RealSpeed = atoi(status);
}

// Parses the STATUS2: string from the PC
// string format: offsetX,offsetY,offsetZ
void ParseStatus2( const char *status )
{
	g_bJobRunning = status[0] == 'J';
	if (g_bJobRunning) status++;
	g_bRecentlyHomed = status[0] == 'H';
	if (g_bRecentlyHomed) status++;
	g_TloState = status[0] - '0';
	status++;
	g_OffsetX = atof(status); status = strchr(status, ',') + 1;
	g_OffsetY = atof(status); status = strchr(status, ',') + 1;
	g_OffsetZ = atof(status);
}

// Parses the UNITS: string from the PC
// string format: <I/M>|<jog1>|<jog2>| ... up to 5 jog values
void ParseUnits( const char *units )
{
	g_bShowInches = *units == 'I';
	units = strchr(units, '|');
	g_JogScreen.ParseJogSteps(units);
}

// Handles the PEN handshake prompt from the PC. responds with DENT:<version> and the current ROM settings
void HandleHandshake( void )
{
	Serial.print(ROMSTR("DANT:"));
	Serial.println(ROMSTR(PENDANT_VERSION));
	Serial.print(ROMSTR("NAME:"));
	Serial.println(g_RomSettings.pendantName);
	Serial.print(ROMSTR("CALIBRATION:"));
	for (uint8_t i = 0; i < 7; i++)
	{
		Serial.print(g_RomSettings.calibration[i]);
		Serial.print(g_StrComma);
	}
	Serial.println(g_RomSettings.calibration[7]);
	g_LastPingTime = g_LastPongTime = g_CurrentTime;
}

// Processes a command from the PC
void ProcessCommand( const char *command, unsigned long time )
{
	// handshake
	if (strcmp(command, "PEN") == 0)
	{
		HandleHandshake();
		return;
	}
	if (strcmp(command, "BYE") == 0)
	{
		g_bConnected =false;
		return;
	}

	// heartbeat
	if (strcmp(command, "PONG") == 0)
	{
		g_LastPongTime = time;
		return;
	}

	// status
	if (strncmp(command, "STATUS:", 7) == 0)
	{
		g_bConnected = true;
		ParseStatus(command + 7);
		return;
	}

	// status2
	if (strncmp(command, "STATUS2:", 8) == 0)
	{
		ParseStatus2(command + 8);
		return;
	}

	//settings
	if (strncmp(command, "UNITS:", 6) == 0)
	{
		ParseUnits(command + 6);
		return;
	}
	if (strncmp(command, "MACROS:", 7) == 0)
	{
#ifndef DISABLE_MACRO_SCREEN
		g_MacroScreen.ParseMacros(command + 7);
#endif
		return;
	}

	if (strncmp(command, "CAL:", 4) == 0)
	{
#ifndef DISABLE_CALIBRATION_SCREEN
		g_CalibrationScreen.ProcessCommand(command + 4, time);
#endif
		return;
	}

	if (strncmp(command, "NAME:", 5) == 0)
	{
		ParseName(command + 5);
		return;
	}

	if (strncmp(command, "CALIBRATION:", 12) == 0)
	{
		ParseCalibration(command + 12);
		return;
	}

	if (strncmp(command, "DIALOG:", 7) == 0)
	{
		g_DialogScreen.Activate(time);
		g_DialogScreen.ParseDialog(command + 7);
		return;
	}

	if (strcmp(command, "JOBSCREEN") == 0)
	{
		g_RunScreen.Activate(time);
		return;
	}

	if (strncmp(command, "PROBESCREEN:", 12) == 0)
	{
		g_ZProbeScreen.Activate(time, (ZProbeScreen::ProbeMode)(command[12] - '0'), false);
		return;
	}
}

void setup( void )
{
#ifndef EMULATOR
	// HACK: 4808-based Arduino clones have I2C on pins D4 and D5 instead of A4 and A5. To support both on the same
	// PCB, I have the pairs shorted together. For safety, the unused pair needs to be marked as INPUT. So
	// here initialize all 4 pins as inputs to avoid interfering with I2C.
	pinMode(A4, INPUT);
	pinMode(A5, INPUT);
	pinMode(4, INPUT);
	pinMode(5, INPUT);
#endif
	Serial.begin(PENDANT_BAUD_RATE);
	InitializeInput();
	InitializeStatusStrings();
	ReadRomSettings();
	u8g2.begin();
	InitializeEncoder();
	g_CurrentTime = millis();
	g_WelcomeScreen.Activate(g_CurrentTime);
}

int g_Frame = 0;
uint16_t g_Dtt = 0;
uint16_t g_DtMax = 0;

void loop( void )
{
	unsigned long time = millis();
	if (time == 0) time = 1; // skip time 0, so 0 can be used as uninitialized value
	uint16_t dt = (uint16_t)(time - g_CurrentTime);
	g_CurrentTime = time;

	g_Dtt += dt;
	if (g_DtMax < dt) g_DtMax = dt;

	// process connection status
	if (g_bConnected)
	{
		unsigned long t = time - g_LastPingTime;
		if (t > PING_TIME)
		{
			if (g_LastPongTime < g_LastPingTime)
			{
				g_bConnected = false;
			}
			else
			{
				Serial.println("PING");
				g_LastPingTime = time;
			}
		}
	}

	const char *command = ProcessSerial();
	if (command)
	{
		// process command
#ifdef EMULATOR
		if (strcmp(command, "PONG") != 0)
		{
			Serial.OutputConsole("[PC] ");
			Serial.OutputConsole(command);
			Serial.OutputConsole("\n");
		}
#endif
		ProcessCommand(command, time);
	}

	// select a screen based on the global status
	bool bScreenSelected = true;
	if (!g_bConnected || g_MachineStatus == STATUS_DISCONNECTED)
	{
#ifndef DISABLE_CALIBRATION_SCREEN
		if (g_CalibrationScreen.IsActive())
		{
			bScreenSelected = false;
		}
		else
#endif
		{
			g_WelcomeScreen.Activate(time);
		}
	}
	else if (g_MachineStatus == STATUS_ALARM)
	{
		if (!g_AlarmScreen.IsActive())
			g_AlarmScreen.Activate(time);
	}
	else
	{
		bScreenSelected = false;
	}

	if (g_MachineStatus >= STATUS_IDLE)
	{
		g_LastIdleTime = time;
	}
	else
	{
		g_LastBusyTime = time;
	}

	g_bCanShowStop = (time - g_LastIdleTime > SHOW_STOP_TIME);

	// read buttons
#ifdef EMULATOR
	uint16_t physicalButtons = g_PhysicalButtons;
#else
	uint16_t physicalButtons = ReadButtons();
#endif
	UpdateButtonState(physicalButtons, dt);
	UpdateJoystick();

	// check for Abort button
	if (TestBit(g_ButtonClick, BUTTON_ABORT))
	{
		Serial.println(ROMSTR("ABORT"));
		if (!bScreenSelected)
		{
			g_MainScreen.Activate(time);
		}
	}

#ifndef DISABLE_CALIBRATION_SCREEN
	if (g_CalibrationScreen.ShouldSendXY())
	{
		g_CalibrationScreen.SendXYUpdate(false);
	}
#endif

	// update current screen
	BaseScreen::s_pCurrentScreen->Update(time);

	// draw
#if U8G2_FULL_BUFFER
	BaseScreen::ClearScreen();
	SetColorIndex(1);
	BaseScreen::s_pCurrentScreen->Draw();
#if PARTIAL_SCREEN_UPDATE
	BaseScreen::UpdateScreen();
#else
	u8g2.sendBuffer();
#endif
#else
	u8g2.firstPage();
	do
	{
		SetColorIndex(1);
		BaseScreen::s_pCurrentScreen->Draw();
	}
	while (u8g2.nextPage());
#endif

	g_Frame++;
	if (g_Frame == 30)
	{
/*		Serial.print("FPS: ");
		Serial.print(g_Dtt/30);
		Serial.print(" MAX: ");
		Serial.println(g_DtMax);*/
		g_Frame=g_Dtt=g_DtMax=0;
	}
}
