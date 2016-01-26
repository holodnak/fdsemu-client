#include "DiskImage.h"

//load disk image data buffer, autodetect image format
bool CDiskImage::LoadFds(uint8_t *buf, int len)
{
	return(true);
}

//load disk image data buffer, autodetect image format
bool CDiskImage::LoadBin(uint8_t *buf, int len)
{
	return(true);
}

//load disk image data buffer, autodetect image format
bool CDiskImage::LoadRaw(uint8_t *buf, int len)
{
	return(true);
}

CDiskImage::CDiskImage()
{
	sides = 0;
	numsides = 0;
}

CDiskImage::~CDiskImage()
{
	if (sides) {

	}
}

bool CDiskImage::Load(char *filename)
{
	return(true);
}


bool CDiskImage::Save(char *filename)
{
	return(true);
}
