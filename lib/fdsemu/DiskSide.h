#pragma once

#include <stdint.h>

class CDiskSide
{
private:
	uint8_t *fds, *bin;
	uint8_t *raw, *raw03;

public:
	CDiskSide();
	virtual ~CDiskSide();

	//load data
	void LoadFds(uint8_t *buf, int len);
};
