#pragma once

#define USE_SHARED_STATE 1 // 0 - each screen stores its own state. 1 - some large screens share memory (potentially slower code?)

#if !USE_SHARED_STATE
#define GetActiveState() (Assert(IsActive()), this)
#endif

///////////////////////////////////////////////////////////////////////////////

// This screen shows when there is an active alarm. Has a DISMISS button to clear the alarm
class AlarmScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Activate( unsigned long time ) override;

private:
	enum
	{
		BUTTON_DISMISS = 7,
	};

	static const int DISMISS_TIMER = 1300; // wait 1.3 seconds after the alarm is cleared to close the dialog

	unsigned long m_DismissTime; // the time of the first non-alarm frame after clicking the button
	uint8_t m_bDismissed : 1; // the button was clicked

private:
#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		uint8_t state;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// This screen shows when the PC is calibrating the joystick
class CalibrationScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Activate( unsigned long time ) override;
	virtual void Deactivate( void ) override;

	// Processes the CAL: commands from the PC
	void ProcessCommand( const char *command, unsigned long time );

	// Sends raw joystick position to the PC if the values have changed or if bForce
	void SendXYUpdate( bool bForce );

	// Returns true if the PC has requested joystick position
	bool ShouldSendXY( void ) const { return m_bSendXY; }

private:
	// previous raw joystick position
	int16_t m_OldJoyX;
	int16_t m_OldJoyY;

	uint8_t m_bSendXY : 1;
	uint8_t m_Stage : 4; // 0 - calibrate range. 1..8 - tests to calibrate deadzone

	enum
	{
		BUTTON_OK = 3,
		BUTTON_BACK = 7,
		DEADZONE_COUNT = 8,
	};

	void Close( unsigned long time );

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		uint8_t stage;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// Generic dialog screen, controlled via a string from PC
class DialogScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Deactivate( void ) override;

// Parses the dialog description: <dialog id>|<title>|<line1>|<line2>|<line3>|<left button>,<right button>
	void ParseDialog( const char *str );

	// Sends a response to the PC
	// DIALOG:<dialog id>|<value>
	// value is: 1 - left button, 2 - right button, 0 - dialog closed for other reasons (alarm? disconnect?)
	void SendResponse( uint8_t button );

private:
	static const int MAX_BUTTON_SIZE = 14;

	void DrawLine( uint8_t row, bool bCenter );

#if USE_SHARED_STATE
	struct ActiveState
	{
#endif

		uint16_t m_Id;
		char m_Lines[4][19];
		char m_Buttons[2][MAX_BUTTON_SIZE + 1];
		uint8_t m_CheckFlags : 3; // lines with unchecked checkboxes
		uint8_t m_AlignFlags : 3; // lines aligned to the left
		uint8_t m_ButtonHoldFlags : 2;
		uint8_t m_ButtonWaitFlags : 2;
		unsigned long m_ButtonDismissTimer;

#if USE_SHARED_STATE
	};

	friend union ScreenTimeshare;
	ActiveState *GetActiveState( void );
#endif

	enum
	{
		BUTTON_1 = 3,
		BUTTON_2 = 7,
	};

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		uint16_t id;
		uint8_t checkFlags;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// This screen allows for jogging the X/Y/Z axis with the wheel and the joystick
class JogScreen : public BaseScreen
{
public:
	JogScreen( void );
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Activate( unsigned long time ) override;
	virtual void Deactivate( void ) override;

	void SetAxis( uint8_t axis );

	// Parses the jog step rate string from the PC - |<rate1>|<rate2> ... - up to 5
	void ParseJogSteps( const char *str );

private:
	static const uint16_t JOG_INACTIVITY_TIMER = 10000; // 10 seconds of inactivity will exit the jog screen

#if USE_SHARED_STATE
	struct ActiveState
	{
#endif

		unsigned long m_LastInputTime; // time of last user input
		unsigned long m_StepHoldTime; // duration of holding the Step button
		unsigned long m_LastWheelTime; // time of the last sent wheel message
		unsigned long m_LastJoystickTime; // time of last sent joystick message
		uint8_t m_Axis : 4; // current axis - one bit for X, Y and Z
		uint8_t m_bShowStop : 1; // show the stop button
		uint8_t m_bShowActions : 1; // show the actions to do during idle
		uint8_t m_bShowAlign : 1; // show Align instead of Step

		// previous quantized joystick position
		int8_t m_OldJoyX;
		int8_t m_OldJoyY;

#if USE_SHARED_STATE
	};

	friend union ScreenTimeshare;
	ActiveState *GetActiveState( void );
#endif

	uint8_t m_StepIndex : 3; // current step rate index
	uint8_t m_StepRateCount : 3;
	uint16_t m_StepRates[5];

	enum
	{
		BUTTON_X = 0,
		BUTTON_Y = 1,
		BUTTON_Z = 2,
		BUTTON_STEP = 3,
		BUTTON_WCS = 4,
		BUTTON_GOTO0 = 5,
		BUTTON_STOP = 5,
		BUTTON_SET0 = 6,
		BUTTON_BACK = 7,
	};

	static void GetJoystick( int8_t *px, int8_t *py );
	friend union ScreenTimeshare;

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		float wx, wy, wz;
		float ox, oy, oz;
		bool bWorkSpace;
		bool bShowInches;
		uint8_t axis : 4;
		uint8_t bShowStop : 1;
		uint8_t bShowActions : 1;
		uint8_t bShowAlign : 1;
		uint8_t stepIndex : 3;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// This screen allows activating custom macros
class MacroScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;

	// Parses the MACROS: string from the PC
	// <hold flags>|<macro1>|<macro2>|<macro3>|<macro4>|<macro5>|<macro6>|<macro7>|
	void ParseMacros( const char *str );

private:
	char m_MacroNames[7][10];
	uint8_t m_UnusedMacros; // 1 bit for each macro with no name
	uint8_t m_MacroHoldFlags; // 1 bit for each macro that requires button hold

	enum
	{
		BUTTON_BACK = 7,
	};
};

///////////////////////////////////////////////////////////////////////////////

// Default screen - shows status and allows access to homing and sub-menus
class MainScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;

private:
	enum
	{
		BUTTON_X = 0,
		BUTTON_Y = 1,
		BUTTON_Z = 2,
		BUTTON_HOME = 3,
		BUTTON_WCS = 4,
		BUTTON_PROBE = 5,
		BUTTON_STOP = 5,
		BUTTON_JOB = 6,
		BUTTON_MACROS = 7,
	};

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		float wx, wy, wz;
		float ox, oy, oz;
		bool bWorkSpace;
		bool bShowInches;
		bool bCanShowStop;
		bool bJobRunning;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// Sub-menu for probing functionality
class ProbeMenuScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;

private:
	enum
	{
		BUTTON_PROBE_Z = 0,
		BUTTON_PROBE_REF_TOOL = 1,
		BUTTON_PROBE_NEW_TOOL = 2,
		BUTTON_BACK = 7,
	};

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		uint8_t tloState;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// Sub-menu for running a job
class RunScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Activate( unsigned long time ) override;

	// Called when the status is Run but the screen is not active
	void RunDetected( void );

private:
	enum
	{
		BUTTON_FEED = 3,
		BUTTON_SPEED = 7,

		BUTTON_RUN = 4,
		BUTTON_BACK = 5,
		BUTTON_PAUSE = 4,
		BUTTON_RESUME = 4,
		BUTTON_STOP = 5,
		BUTTON_RPM0 = 6,
	};

	enum ScreenState
	{
		STATE_IDLE, // job hasn't started, ready to run
		STATE_RUNNING, // job is currently running
		STATE_PAUSING, // job is pausing
		STATE_PAUSED, // job is paused, but not clear to resume
		STATE_PAUSED_READY, // job is ready to resome
		STATE_OTHER, // don't know what to do
	};

	enum JobState
	{
		JOB_NOT_STARTED,
		JOB_STARTED,
		JOB_STOPPED,
	};

	// Converts te machine status to a screen state
	ScreenState DecodeState( void ) const;

	const uint16_t OVERRIDE_TIMER = 10000; // dismiss the override if not used for 10 seconds
	const uint16_t REAL_VALUE_TIMER = 10000; // show the real value 2 seconds after the last change

	unsigned long m_OverrideTimer;
	uint8_t m_Override : 4; // 0, BUTTON_FEED, BUTTON_SPEED
	uint8_t m_JobState : 4;

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		float x, y, z;
		uint16_t f, s;
		uint16_t rf, rs;
		bool bShowInches;
		ScreenState screenState;
		uint8_t _override;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// This screen shows when the CNC is disconnected
class WelcomeScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		char name[19];
		bool bConnected;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

// This screen handles the Z probing and tool measurement
class ZProbeScreen : public BaseScreen
{
public:
	virtual void Draw( void ) override;
	virtual void Update( unsigned long time ) override;
	virtual void Deactivate( void ) override;

	enum ProbeMode
	{
		PROBE_Z,
		PROBE_REF_TOOL,
		PROBE_NEW_TOOL,
	};

	void Activate( unsigned long time, ProbeMode mode, bool bNotify );

	enum
	{
		BUTTON_CONNECT = 0,
		BUTTON_UP = 1,
		BUTTON_DOWN = 2,
		BUTTON_PROBE = 6,
		BUTTON_STOP = 5,
		BUTTON_BACK = 7,
	};

	static const int ZPROBE_INACTIVITY_TIMER = 30000; // 30 seconds of inactivity will exit the jog screen

	uint8_t m_bConfirmed : 1;
	uint8_t m_bJoggingLocked : 1;
	uint8_t m_bJoggingUp : 1;
	uint8_t m_bJoggingDown : 1;
	uint8_t m_ProbeMode : 2;
	unsigned long m_LastInputTime;

#if PARTIAL_SCREEN_UPDATE
	struct DrawState
	{
		uint8_t unusedButtons;
		uint8_t bJoggingUp : 1;
		uint8_t bJoggingDown : 1;
	};

	static_assert(sizeof(DrawState) <= sizeof(DrawStateBase::custom), "draw state too big");
#endif
};

///////////////////////////////////////////////////////////////////////////////

#if USE_SHARED_STATE
union ScreenTimeshare
{
	DialogScreen::ActiveState dialog;
	JogScreen::ActiveState jog;
};

ScreenTimeshare g_ScreenTimeshare;
#endif
