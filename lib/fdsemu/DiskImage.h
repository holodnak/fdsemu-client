#pragma once

#include "DiskSide.h"

class CDiskImage
{
private:

protected:

	//disk sides
	CDiskSide *sides;

	//number of disk sides
	int numsides;

protected:

	//load disk image data buffer, autodetect image format
	bool LoadFds(uint8_t *buf, int len);

	//load disk image data buffer, autodetect image format
	bool LoadBin(uint8_t *buf, int len);

	//load disk image data buffer, autodetect image format
	bool LoadRaw(uint8_t *buf, int len);

public:
	CDiskImage();
	virtual ~CDiskImage();

	//load disk image from file
	bool Load(char *filename);

	//save disk image to file, fwNES format only
	bool Save(char *filename);
};
