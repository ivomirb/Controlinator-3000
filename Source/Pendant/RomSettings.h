#pragma once

const uint16_t SETTINGS_SIGNATURE = 37152;
const char g_StrDefaultName[] PROGMEM = "Controlinator 3000";

// Persistent settings stored in the pendant's ROM
struct RomSettings
{
	uint16_t signature; // must match SETTINGS_SIGNATURE
	uint16_t calibration[8];
	char pendantName[19];
#if USE_WATCHDOG
	uint8_t bCrash;
#endif
};

const int SETTINGS_ROM_ADDRESS = 0;
RomSettings g_RomSettings;

///////////////////////////////////////////////////////////////////////////////

//Reads the settings from the ROM
void ReadRomSettings( void )
{
	EEPROM.get(SETTINGS_ROM_ADDRESS, g_RomSettings);
	if (g_RomSettings.signature != SETTINGS_SIGNATURE)
	{
		g_RomSettings.signature = SETTINGS_SIGNATURE;
		strcpy_P(g_RomSettings.pendantName, g_StrDefaultName);
		g_RomSettings.calibration[0] = 0;
		g_RomSettings.calibration[1] = 512-64;
		g_RomSettings.calibration[2] = 512+64;
		g_RomSettings.calibration[3] = 1023;
		g_RomSettings.calibration[4] = 0;
		g_RomSettings.calibration[5] = 512-64;
		g_RomSettings.calibration[6] = 512+64;
		g_RomSettings.calibration[7] = 1023;
#if USE_WATCHDOG
		g_RomSettings.bCrash = 0;
#endif
		EEPROM.put(SETTINGS_ROM_ADDRESS, g_RomSettings);
	}
}

// Parses the NAME: string from the PC and stores the name in the ROM
void ParseName( const char *name )
{
	int16_t len = Strlen(name);
	if (len > 18) len = 18;
	memcpy(g_RomSettings.pendantName, name, len);
	g_RomSettings.pendantName[len] = 0;
	EEPROM.put(SETTINGS_ROM_ADDRESS, g_RomSettings);
}

// Parses the CALIBRATION: string from the PC and stores the settings in the ROM
// string format: <min x>,<max x>,<min deadx>,<max deadx>,<min y>,<max y>,<min deady>,<max deady>
void ParseCalibration( const char *str )
{
	for (uint8_t i = 0; i < 8; i++)
	{
		g_RomSettings.calibration[i] = atoi(str);
		const char *end = strchr(str, ',');
		if (!end) break;
		str = end + 1;
	}

	if (g_RomSettings.calibration[1] <= g_RomSettings.calibration[0])
		g_RomSettings.calibration[1] = g_RomSettings.calibration[0] + 1;
	if (g_RomSettings.calibration[2] <= g_RomSettings.calibration[1])
		g_RomSettings.calibration[2] = g_RomSettings.calibration[1] + 1;
	if (g_RomSettings.calibration[3] <= g_RomSettings.calibration[2])
		g_RomSettings.calibration[3] = g_RomSettings.calibration[2] + 1;

	if (g_RomSettings.calibration[5] <= g_RomSettings.calibration[4])
		g_RomSettings.calibration[5] = g_RomSettings.calibration[4] + 1;
	if (g_RomSettings.calibration[6] <= g_RomSettings.calibration[5])
		g_RomSettings.calibration[6] = g_RomSettings.calibration[5] + 1;
	if (g_RomSettings.calibration[7] <= g_RomSettings.calibration[6])
		g_RomSettings.calibration[7] = g_RomSettings.calibration[6] + 1;

	EEPROM.put(SETTINGS_ROM_ADDRESS, g_RomSettings);
}
