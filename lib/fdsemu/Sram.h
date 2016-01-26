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
	bool Read(uint8_t *buf, uint32_t addr, int size);
	bool Write(uint8_t *buf, uint32_t addr, int size);
};

