#pragma once

#include <stdint.h>
#include "Device.h"

class CSram
{
protected:
	CDevice *dev;

public:
	CSram(CDevice *d);
	virtual ~CSram();

	//read and write to flash
	virtual bool Read(uint8_t *buf, uint32_t addr, int size);
	virtual bool Write(uint8_t *buf, uint32_t addr, int size);
	bool Transfer(uint32_t slot);
};

class CSramV2 : public CSram 
{
public:
	CSramV2(CDevice *d);

	//read and write to flash
	bool Read(uint8_t *buf, uint32_t addr, int size);
	bool Write(uint8_t *buf, uint32_t addr, int size);
};

