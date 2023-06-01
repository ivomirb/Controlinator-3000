#pragma once

class PersistentStorage
{
public:
	PersistentStorage( void );
	unsigned char read( int address );
	void update( int address, unsigned char byte );

	template<class T> void get( int address, T &t )
	{
		unsigned char *ptr = reinterpret_cast<unsigned char*>(&t);
		for (int i = 0; i < sizeof(T); i++)
		{
			ptr[i] = read(address + i);
		}
	}

	template<class T> void put( int address, const T &t )
	{
		const unsigned char *ptr = reinterpret_cast<const unsigned char*>(&t);
		for (int i = 0; i < sizeof(T); i++)
		{
			update(address + i, ptr[i]);
		}
	}

private:
	static const int DATA_SIZE = 128;

	FILE *m_File;
	unsigned char m_Data[DATA_SIZE];
};

extern PersistentStorage EEPROM;
