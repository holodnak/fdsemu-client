#include <string.h>
#include <stdlib.h>
#include "DiskSide.h"
#include "diskutil.h"
#include "Device.h"

CDiskSide::CDiskSide()
{
	raw = 0;
	raw03 = 0;
	bin = 0;
	rawsize = 0;
	binsize = 0;
	issavedisk = 0;
}

CDiskSide::~CDiskSide()
{
	if (raw) {
		free(raw);
	}

	if (raw03) {
		free(raw03);
	}

	if (bin) {
		free(bin);
	}

}

const int BINSIZE = 0x18000;
const int LEAD_IN = DEFAULT_LEAD_IN / 8;

bool CDiskSide::LoadFDS(uint8_t *buf, int bufsize)
{

	//allocate buffer
	bin = (uint8_t*)malloc(BINSIZE);
	memset(bin, 0, BINSIZE);

	//convert disk to bin format
	binsize = fds_to_bin(bin + LEAD_IN, buf, BINSIZE - LEAD_IN);

	//check if conversion was successful
	if (binsize == 0) {
		free(bin);
		bin = 0;
		return(false);
	}

	//add lead-in size to converted image size and advance input pointer
	binsize += LEAD_IN;

	return(true);
}

bool CDiskSide::LoadGD(uint8_t *buf, int bufsize)
{
	const int TMPSIZE = 0x20000;
	uint8_t *tmp;

	tmp = (uint8_t*)malloc(TMPSIZE);

	//copy to the larger data block
	memset(tmp, 0, TMPSIZE);
	memcpy(tmp, buf, bufsize);

	//allocate buffer and clear it
	bin = (uint8_t*)malloc(BINSIZE);
	memset(bin, 0, BINSIZE);

	//convert disk to bin format
	binsize = gameDoctor_to_bin(bin + LEAD_IN, tmp, BINSIZE - LEAD_IN);

	//free tmp buffer
	free(tmp);

	//check if conversion was successful
	if (binsize == 0) {
//		printf("Error converting gamedoctor disk image bin\n");
		free(bin);
		bin = 0;
		return(false);
	}

	//account for lead-in data
	binsize += LEAD_IN;

	return(true);
}

bool CDiskSide::Duplicate(CDiskSide *side)
{
	rawsize = side->rawsize;
	binsize = side->binsize;
	if (side->raw) {
		raw = (uint8_t*)malloc(rawsize);
		memcpy(raw, side->raw, rawsize);
	}
	if (side->raw03) {
		raw03 = (uint8_t*)malloc(rawsize);
		memcpy(raw03, side->raw03, rawsize);
	}
	if (side->bin) {
		bin = (uint8_t*)malloc(binsize);
		memcpy(bin, side->bin, binsize);
	}
	return(true);
}

bool CDiskSide::GetBin(uint8_t **buf, int *bufsize)
{
	//convert raw to bin if necessary
	if (bin == 0) {
		if (raw03 == 0) {
			if (raw == 0) {
				return(false);
			}
			raw03 = (uint8_t*)malloc(rawsize);
			memcpy(raw03, raw, rawsize);
			raw_to_raw03(raw03, rawsize);
		}
		raw03 = (uint8_t*)malloc(rawsize);
		raw03_to_bin(raw03, rawsize, &bin, &binsize, &datasize);
	}

	*buf = bin;
	*bufsize = binsize;
	return(true);
}
