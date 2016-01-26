#include <stdio.h>
#include "Sram.h"

enum {
	CMD_WRITEDATA = 2,
};

CSram::CSram(CDevice *d)
{
	dev = d;
}

CSram::~CSram()
{
}

bool CSram::Read(uint8_t *buf, uint32_t addr, int size)
{
	uint8_t cmd[4] = { CMD_READDATA, 0, 0, 0 };

	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr;
	if (!dev->SramWrite(cmd, 4, 1, 1))
		return false;
	for (; size>0; size -= SPI_READMAX) {
		if (!dev->SramRead(buf, size>SPI_READMAX ? SPI_READMAX : size, size>SPI_READMAX))
			return false;
		buf += SPI_READMAX;
	}
	return true;
}

bool CSram::Write(uint8_t *buf, uint32_t addr, int size)
{
	static uint8_t cmd[4] = { CMD_WRITEDATA,0,0,0 };
	cmd[2] = addr >> 8;
	cmd[3] = addr;

	//	printf("outputting write command\n");
	if (!dev->SramWrite(cmd, 3, 1, 1)) {
		printf("CSram::Write: SramWrite failed.\n");
		return false;
	}

	for (; size>0; size -= SPI_WRITEMAX) {
		if (!dev->SramWrite((uint8_t*)buf, size>SPI_WRITEMAX ? SPI_WRITEMAX : size, 0, size>SPI_WRITEMAX))
			return(false);
		buf += SPI_WRITEMAX;
	}
	return(true);
}
