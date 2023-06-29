#pragma once

// machine status
enum MachineStatus
{
	STATUS_UNKNOWN, // none of the below
	STATUS_DISCONNECTED, // machine not connected

	STATUS_JOG,
	STATUS_RUN,
	STATUS_CHECK, // what's this?
	STATUS_HOME, // never happens?
	STATUS_RUNNING, // from OB CONTROL
	STATUS_RESUMING, // from OB CONTROL
	STATUS_DOOR3_RESUMING, // door:3 - door closed, resuming in progress

	// the statuses below are considered "inactive"
	STATUS_IDLE,
	STATUS_HOLD0_COMPLETE, // hold:0
	STATUS_HOLD1_STOPPING, // hold:1 - hold in progress
	STATUS_DOOR0_CLOSED,   // door:0 - door closed, ready to resume
	STATUS_DOOR1_OPENED,   // door:1 - door opened, holding
	STATUS_DOOR2_STOPPING, // door:2 - door opened, stopping in progress
	STATUS_ALARM,
	STATUS_SLEEP,
	STATUS_STOPPED, // from OB CONTROL
	STATUS_PAUSED, // from OB CONTROL

	STATUS_COUNT,
};

MachineStatus g_MachineStatus = STATUS_UNKNOWN;

// This table match the names and order in Javascript
const char g_StatusNames[] PROGMEM = 
{
	"???\0"
	"???\0"

	"Jog\0"
	"Run\0"
	"Check\0"
	"Home\0"
	"Running\0"
	"Resuming\0"
	"Door:3\0"

	"Idle\0"
	"Hold:0\0"
	"Hold:1\0"
	"Door:0\0"
	"Door:1\0"
	"Door:2\0"
	"Alarm\0"
	"Sleep\0"
	"Stopped\0"
	"Paused\0"
};

static_assert(sizeof(g_StatusNames) < 256, "");

uint8_t g_StatusNameOffsets[STATUS_COUNT + 1];

void InitializeStatusStrings( void )
{
	uint8_t offset = 0;
	for (uint8_t i = 0; i < STATUS_COUNT; i++)
	{
		g_StatusNameOffsets[i] = offset;
		offset += strlen_P(g_StatusNames + offset) + 1;
	}
	g_StatusNameOffsets[STATUS_COUNT] = offset;
}

#ifdef EMULATOR
const char *GetStatusName( MachineStatus status )
{
	return g_StatusNames + g_StatusNameOffsets[status];
}
#else
const __FlashStringHelper *GetStatusName( MachineStatus status )
{
	return reinterpret_cast<const __FlashStringHelper*>(g_StatusNames + g_StatusNameOffsets[status]);
}
#endif
int8_t GetStatusNameLen( MachineStatus status )
{
	return g_StatusNameOffsets[status + 1] - g_StatusNameOffsets[status] - 1;
}
