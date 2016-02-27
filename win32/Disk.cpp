#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "Disk.h"
#include "Device.h"
#include "Fdsemu.h"
#include "flashrw.h"

int detect_format(char *filename)
{
	FILE *fp;
	uint8_t buf[128];
	int ret = FORMAT_UNKNOWN;

	printf("Writing disk image '%s'...\n", filename);
	if ((fp = fopen(filename, "rb")) == 0) {
		printf("error opening file: %s\n", filename);
	}
	else if (fread(buf, 1, 128, fp) != 128) {
		printf("error reading disk image\n");
	}
	else {

		//detect fds format
		if (buf[0] == 'F' && buf[1] == 'D' && buf[2] == 'S' && buf[3] == 0x1A) {
			printf("Detected fwNES format.\n");
			ret = FORMAT_FDS;// FDS_writeDisk(filename);
		}

		else if (buf[0] == 0x01 && buf[1] == 0x2A && buf[2] == 0x4E && buf[0x38] == 0x02) {
			printf("Detected FDS format.\n");
			ret = FORMAT_FDS;// FDS_writeDisk(filename);
		}

		//detect game doctor format
		else if (buf[3] == 0x01 || buf[4] == 0x2A || buf[5] == 0x4E || buf[0x3D] == 0x02) {
			printf("Detected Game Doctor format.\n");
			ret = FORMAT_GD;// GD_writeDisk(filename);
		}

		else {
			printf("unknown disk image format\n");
		}
	}
	fclose(fp);
	return(ret);
}

CDisk::CDisk()
{
	memset(sides, 0, sizeof(CDiskSide*) * MAX_SIDES);
	numsides = 0;
}

CDisk::~CDisk()
{
	FreeSides();
}

void CDisk::FreeSides()
{
	int i;

	for (i = 0; i < numsides; i++) {
		delete sides[i];
	}
	memset(sides, 0, sizeof(CDiskSide*) * MAX_SIDES);
	numsides = 0;
}

bool CDisk::LoadFDS(char *filename)
{
	uint8_t *buf, *ptr;
	int filesize, i;
	CDiskSide *side;

	//load entire file into a buffer
	if (!loadfile(filename, &buf, &filesize)) {
		printf("Error loading %s\n", filename);
		return(false);
	}

	//load success
	ptr = buf;
	if (ptr[0] == 'F' && ptr[1] == 'D' && ptr[2] == 'S' && ptr[3] == 0x1A) {
		ptr += 16;
		filesize -= (filesize - 16) % FDSSIZE;  //truncate down to whole disk
	}
	else {
		filesize -= filesize % FDSSIZE;  //truncate down to whole disk
	}

	//fill sides array with disk data
	for (i = 0; i < (filesize / FDSSIZE); i++) {

		side = new CDiskSide();
		if (side->LoadFDS(ptr, filesize - (i * FDSSIZE)) == false) {
			delete side;
			break;
		}

		sides[numsides++] = side;

		ptr += FDSSIZE;
	}

	free(buf);
	return(true);
}

bool CDisk::LoadGD(char *filename)
{
	const int DISKSIZE = 0x18000;
	char *files[MAX_SIDES + 1], *ptr;
	int numfiles = 0;
	uint8_t *buf = 0;
	uint8_t *binbuf = 0;
	int i, len;
	CDiskSide *side;

	//find files for this disk image
	if (find_doctors(filename, files, &numfiles, MAX_SIDES + 1) == false) {
		printf("Failed to find doctor disk sides\n");
		return(false);
	}

	//allocate temporary space for storing the bins
	binbuf = (uint8_t*)malloc(DISKSIZE);

	//loop thru found files
	for (i = 0; i < numfiles; i++) {
		ptr = files[i];
		printf("Loading %s...", ptr);

		//load game doctor image file
		if (loadfile(ptr, &buf, &len) == false) {
			printf("Error loading file\n");
			break;
		}

		side = new CDiskSide();
		if (side->LoadGD(buf, len) == false) {
			delete side;
			break;
		}

		//check for save disk extension
		if (toupper(ptr[strlen(ptr) - 1]) == 'S') {
			side->SetSaveDisk(1);
		}

		free(buf);
		sides[numsides++] = side;
	}

	free(binbuf);

	if (i == numfiles) {
		return(true);
	}
	return(false);
}

bool CDisk::Load(char *filename)
{
	int format = detect_format(filename);

	switch (format) {
	case FORMAT_FDS:
		return(LoadFDS(filename));
	case FORMAT_GD:
		return(LoadGD(filename));
	default:
		printf("unknown format for %s\n", filename);
		break;
	}
	return(false);
}

bool CDisk::Load(CDiskSide *side)
{
	CDiskSide *sidecopy;

	sidecopy = new CDiskSide();
	sidecopy->Duplicate(side);
	sides[numsides++] = sidecopy;
	return(true);
}

bool CDisk::GetBin(int side, uint8_t **bin, int *binsize)
{
	if (side >= numsides) {
		return(false);
	}

	return(sides[side]->GetBin(bin, binsize));
}
