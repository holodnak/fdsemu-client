#pragma once

#include <stdint.h>

class CDiskBlock
{
private:
	uint8_t	id;				//block id
	uint8_t	*data;			//data contained in block
	int len;				//length of data (not including the id)

public:
	CDiskBlock();
	virtual ~CDiskBlock();
};

#define MAX_BLOCKS	256

class CDiskSide
{
private:
	CDiskBlock	blocks[MAX_BLOCKS];

public:
	CDiskSide();
	virtual ~CDiskSide();
};
