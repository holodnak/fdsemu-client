#include <stdio.h>
#include <string.h>
#include "Flash.h"
#include "System.h"

bool CFlash::CheckExtendedAddress(uint32_t addr)
{
	uint8_t cmd[4] = { CMD_WRITEEXTADDR, 0, 0, 0 };
	uint8_t tmp = (uint8_t)(addr >> 24);

    //check if extended address bits are necessary
    if(dev->FlashSize <= 0x1000000) {
        return(true);
    }

    if (tmp != ExtAddr) {
		if (WriteEnable() == false) {
			return(false);
		}
		cmd[1] = tmp;
		if (dev->FlashWrite(cmd, 2, 1, 0) == false) {
			return(false);
		}
	}
	ExtAddr = tmp;

/*	cmd[0] = CMD_READEXTADDR;
	dev->FlashWrite(cmd, 1, 1, 1);
	dev->FlashRead(cmd, 1, 0);
    printf("extaddr = %02X (wanted %02X)\n", cmd[0], tmp);
*/
    return(true);
}

CFlash::CFlash(CDevice *d)
{
	ExtAddr = 0;
	dev = d;
	Reset();
}


CFlash::~CFlash()
{
}

bool CFlash::Read(uint8_t *buf, uint32_t addr, int size, TCallback cb, void *user)
{
	uint8_t cmd[4] = { CMD_READDATA, 0, 0, 0 };
    int n;

	if (CheckExtendedAddress(addr) == false) {
		return(false);
	}
	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr;
	if (!dev->FlashWrite(cmd, 4, 1, 1))
		return false;
    for (n = 0; size>0; size -= SPI_READMAX, n += SPI_READMAX) {
		if (!dev->FlashRead(buf, size>SPI_READMAX ? SPI_READMAX : size, size>SPI_READMAX))
			return false;
		buf += SPI_READMAX;
        if((n % (SPI_READMAX * 100)) == 0) {
            if(cb) {
                cb(user, n);
            }
        }
	}
	return true;
}

bool CFlash::Write(uint8_t *buf, uint32_t addr, int size, TCallback cb, void *user)
{
	int i;

	if (size % SECTORSIZE) {
		printf("CFlash::Write:  cannot write data, size must be a multiple of %d\n", SECTORSIZE);
		return(false);
	}

    if (Erase(addr, size) == false) {
        return(false);
    }

	//write pages
	for (i = 0; i < size; i += PAGESIZE) {
		if (PageProgram(addr + i, buf + i) == false) {
			return(false);
		}
		if ((addr + i) % 0x1000 == 0) {
			if (cb) {
				cb(user, i);
			}
		}
	}

	return(true);
}

bool CFlash::Erase(uint32_t addr, int size)
{
	int i;

    //multiple of 64kb
    if((size & 0xFFFF) == 0 && (addr & 0xFFFF) == 0) {
        for(i=0;i<size;i+=0x10000) {
            if(EraseBlock(addr + i) == false) {
                return(false);
            }
        }
        return(true);
    }

	if (size % SECTORSIZE) {
		printf("CFlash::Erase:  cannot erase, size must be a multiple of %d\n", SECTORSIZE);
		return(false);
	}

	//erase sectors
	for (i = 0; i < size; i += SECTORSIZE) {
		if (EraseSector(addr + i) == false) {
			return(false);
		}
	}
	return(true);
}

bool CFlash::WriteEnable() {
	static uint8_t cmd[] = { CMD_WRITEENABLE };

	return(dev->FlashWrite(cmd, 1, 1, 0));
}

bool CFlash::WaitBusy(uint32_t timeout) {
	static uint8_t cmd[] = { CMD_READSTATUS };
	uint8_t status;

	if (!dev->FlashWrite(cmd, 1, 1, 1))
		return false;

	uint32_t start = getTicks();
	do {
		if (!dev->FlashRead(&status, 1, 1))
			return false;
	} while ((status & 1) && (getTicks() - start < timeout));
	if (!dev->FlashWrite(0, 0, 0, 0)) // CS release
		return false;
	return !(status & 1);
}

bool CFlash::PageProgram(uint32_t addr, uint8_t *buf)
{
	uint8_t cmd[PAGESIZE + 4];
	int size = PAGESIZE;

	if (((addr&(PAGESIZE - 1)) + size)>PAGESIZE)	{
		printf("Page write overflow.\n");
		return false;
	}
	if (!WriteEnable()) {
		printf("Write enable failed.\n");
		return false;
	}
	if (CheckExtendedAddress(addr) == false) {
		return(false);
	}
	cmd[0] = CMD_PAGEWRITE;
	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr;
	memcpy(cmd + 4, buf, size);
	size += 4;

	uint8_t *p = cmd;
	for (; size>0; size -= SPI_WRITEMAX) {
		if (!dev->FlashWrite(p, size>SPI_WRITEMAX ? SPI_WRITEMAX : size, p == cmd, size>SPI_WRITEMAX))
			return false;
		p += SPI_WRITEMAX;
	}
	return WaitBusy(100);
}

bool CFlash::EraseSector(uint32_t addr)
{
    uint8_t cmd[] = { CMD_SECTORERASE,0,0,0 };

    if (CheckExtendedAddress(addr) == false) {
        return(false);
    }
    if (!WriteEnable())
        return false;
    cmd[1] = addr >> 16;
    cmd[2] = addr >> 8;
    if (!dev->FlashWrite(cmd, 4, 1, 0))
        return false;
    return WaitBusy(1600);
}

bool CFlash::EraseBlock(uint32_t addr)
{
    uint8_t cmd[] = { CMD_BLOCKERASE,0,0,0 };

    if (CheckExtendedAddress(addr) == false) {
        return(false);
    }
    if (!WriteEnable())
        return false;
    cmd[1] = addr >> 16;
    if (!dev->FlashWrite(cmd, 4, 1, 0))
        return false;
    return WaitBusy(1600);
}

bool CFlash::EraseSlot(int slot)
{
    return(EraseBlock((uint32_t)slot * 0x10000));
}

bool CFlash::ChipErase()
{
	return(false);
}

bool CFlash::Reset()
{
	uint8_t cmd[] = { 0,0,0,0 };

	cmd[0] = 0x66;
	dev->FlashWrite(cmd, 1, 1, 0);
	cmd[0] = 0x99;
	dev->FlashWrite(cmd, 1, 1, 0);
	return(false);
}
