#include "pch.h"
#include "EEPROM.h"
#include <stdio.h>

PersistentStorage EEPROM;

PersistentStorage::PersistentStorage( void )
{
	memset(m_Data, 0xFF, DATA_SIZE);
	m_File = NULL;
	if (fopen_s(&m_File, "EEPROM.bin", "r") == 0 || fopen_s(&m_File, "EEPROM.bin", "w") == 0)
	{
		fclose(m_File);
	}
	m_File = NULL;
	fopen_s(&m_File, "EEPROM.bin", "rb+");
	if (m_File)
	{
		fread(m_Data, 1, DATA_SIZE, m_File);
		fseek(m_File, 0, SEEK_SET);
		fwrite(m_Data, 1, DATA_SIZE, m_File);
	}
}

unsigned char PersistentStorage::read( int address )
{
	if (address < 0 || address >= DATA_SIZE)
		return 0xFF;
	return m_Data[address];
}

void PersistentStorage::update( int address, unsigned char byte )
{
	if (address < 0 || address >= DATA_SIZE)
		return;
	if (m_Data[address] == byte)
		return;

	m_Data[address] = byte;
	if (m_File)
	{
		fseek(m_File, address, SEEK_SET);
		fwrite(&m_Data[address], 1, 1, m_File);
		fflush(m_File);
	}
}
