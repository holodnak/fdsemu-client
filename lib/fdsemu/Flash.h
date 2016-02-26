#pragma once
#include "Device.h"

enum {
//	PAGESIZE = 256,
	CMD_READID = 0x9F,
	CMD_READSTATUS = 0x05,
	CMD_WRITEENABLE = 0x06,

	CMD_READDATA = 0x03,
	CMD_WRITESTATUS = 0x01,
	CMD_PAGEWRITE = 0x02,
	CMD_PAGEERASE = 0xdb,
	CMD_PAGEPROGRAM = 0x02,
    CMD_BLOCKERASE = 0xD8,
	CMD_SECTORERASE = 0x20,
	CMD_CHIPERASE = 0xC7,
	CMD_READEXTADDR = 0xC8,
	CMD_WRITEEXTADDR = 0xC5,

	SECTORSIZE = 4096,
	PAGESIZE = 256,
};

#define CALLBACK_NUMSIDES	0
#define CALLBACK_SIDESTART	1
#define CALLBACK_WRITEADDR	2

typedef void(*TCallback)(void*, int, int);

class CFlash
{
private:
	uint8_t ExtAddr;
protected:
	CDevice *dev;
protected:
	bool CheckExtendedAddress(uint32_t addr);
public:
	CFlash(CDevice *d);
	virtual ~CFlash();

	//enable writes
	virtual bool WriteEnable();

	//wait for chip to stop being busy
	virtual bool WaitBusy(uint32_t timeout);

	//read and write to flash
    virtual bool Read(uint8_t *buf, uint32_t addr, int size, TCallback cb = 0, void *user = 0);
	virtual bool Write(uint8_t *buf, uint32_t addr, int size, TCallback cb = 0, void *user = 0);
	virtual bool Erase(uint32_t addr, int size);

	//write one 256 byte page
    bool PageProgram(uint32_t addr, uint8_t *buf);

    //erase 4kb sector
    bool EraseSector(uint32_t addr);

    //erase 64kb block
    bool EraseBlock(uint32_t addr);

    bool EraseSlot(int slot);

	//erase entire chip
	bool ChipErase();

	//reset chip
	bool Reset();
};
