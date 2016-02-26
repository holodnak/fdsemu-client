#pragma once

#include <stdint.h>
#include "Device.h"

typedef struct SFlashHeader {
	uint8_t filename[240];		//filename of disk in flash (if first slot)
	uint16_t	size;					//size of data stored in the block
	uint16_t	ownerid;				//id of first slot of this game
	uint16_t	nextid;				//id of next block in disk chain
	uint16_t	saveid;				//id of "save disk" for game doctor
	uint8_t flags;					//disk flags cris-00tt
										// c = compressed
										// r = read only
										// i = owner id/next id fields are VALID
										// s = saveid field is VALID
										// t = type
	uint8_t flags2;
	uint8_t reserved[6];
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
