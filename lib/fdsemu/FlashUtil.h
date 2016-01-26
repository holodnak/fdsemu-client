#pragma once

#include <stdint.h>
#include "Device.h"

typedef struct SFlashHeader {
	uint8_t filename[240];		//filename of disk in flash (if first slot)
	uint32_t checksum;			//xor checksum of disk data
	uint16_t leadin;				//leadin size of disk
	uint16_t nextslot;			//slot of next disk side of the disk image
	uint8_t reserved[8];			//reserved for future expansion
} TFlashHeader;

class CFlashUtil
{
protected:
	CDevice *dev;
	TFlashHeader *headers;
	int numslots;

public:
	CFlashUtil(CDevice *d);
	virtual ~CFlashUtil();

	//read all headers and save them to memory
	bool ReadHeaders();

	//return array of flash headers
	TFlashHeader *GetHeaders();
};
