#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "diskutil.h"
#include "System.h"

// TODO - only handles one side, files will need to be joined manually
bool FDS_readDisk(char *filename_raw, char *filename_bin, char *filename_fds) {
	enum { READBUFSIZE = 0x90000 };

	FILE *f;
	uint8_t *readBuf = NULL;
	int result;
	int bytesIn = 0;

	//if(!(dev_readIO()&MEDIA_SET)) {
	//    printf("Warning - Disk not inserted?\n");
	//}
	if (!dev.DiskReadStart()) {
		printf("diskreadstart failed\n");
		return false;
	}

	readBuf = (uint8_t*)malloc(READBUFSIZE);
	do {
		result = dev.DiskRead(readBuf + bytesIn);
		bytesIn += result;
		if (!(bytesIn % ((DISK_READMAX)* 32)))
			printf(".");
	} while (result == DISK_READMAX && bytesIn<READBUFSIZE - DISK_READMAX);
	printf("\n");
	if (result<0) {
		printf("Read error.\n");
		return false;
	}

	if (filename_raw) {
		if ((f = fopen(filename_raw, "wb"))) {
			fwrite(readBuf, 1, bytesIn, f);
			fclose(f);
			printf("Wrote %s\n", filename_raw);
		}
	}

	raw_to_raw03(readBuf, bytesIn);

	//decode to .fds
	if (filename_fds) {
		uint8_t *fds = (uint8_t*)malloc(FDSSIZE + 16 + 0x10000);   //extra room for CRC junk
		raw03_to_fds(readBuf, fds, bytesIn);
		if ((f = fopen(filename_fds, "wb"))) {
			fwrite(fds, 1, FDSSIZE, f);
			fclose(f);
			printf("Wrote %s\n", filename_fds);
		}
		free(fds);

		//decode to .bin
	}
	else if (filename_bin) {
		uint8_t *binBuf;
		int binSize, dataSize;
		char *output;

		raw03_to_bin(readBuf + 3400, bytesIn - 3400, &binBuf, &binSize, &dataSize);
		output = messages_get();
		if (output) {
			printf(output);
		}
		if ((f = fopen(filename_bin, "wb"))) {
			fwrite(binBuf, 1, binSize, f);
			fclose(f);
			printf("Wrote %s\n", filename_bin);
		}
		free(binBuf);
	}

	free(readBuf);
	return true;
}

static bool writeDisk2(uint8_t *bin, int binSize) {
	//	printf("writeDisk2: sending bin image to adaptor, size = %d\n", binSize);

	//		hexdump("bin", bin+ 3537, 256);

	if (dev.Sram->Write(bin, 0, binSize) == false) {
		printf("Sram write failed.\n");
		return(false);
	}

	if (!dev.DiskWriteStart())
		return false;

	return(true);

}

const int LEAD_IN = DEFAULT_LEAD_IN / 8;

typedef struct bin_s {
	uint8_t *data;
	int size;
} bin_t;

/*
writes pre-formatted bin disk sides to disk
*/
bool BIN_writeDisk(bin_t *sides, int numsides)
{
	const int ZEROSIZE = 0x10000 + 8192;
	uint8_t *zero = 0;
	char prompt;
	bool ret = true;

	int i;
	zero = (uint8_t*)malloc(ZEROSIZE);
	memset(zero, 0, ZEROSIZE);

	for (i = 0; i < numsides; i++) {
		printf("Side %d\n", i + 1);

		//zero out the sram
		if (dev.Sram->Write(zero, 0, ZEROSIZE) == false) {
			printf("Sram write failed (zero).\n");
			ret = false;
			break;
		}

		//write disk bin image to sram
		if (dev.Sram->Write(sides[i].data, 0, sides[i].size) == false) {
			printf("Sram write failed (bin).\n");
			ret = false;
			break;
		}

		//tell fdsemu to start writing the disk
		if (!dev.DiskWriteStart()) {
			ret = false;
			break;
		}

		//check for more sides to be written
		if ((i + 1) < numsides) {
			printf("\nPlease wait for disk activity to stop before pressing ENTER to write the next disk side.\nPressing any other key will cancel.\n");
			prompt = readKb();

			//ensure enter keypress
			if (prompt == 0x0D) {
				continue;
			}
			else {
				break;
			}
		}
		else {
			printf("\nDisk image sent to SRAM on device and is currently writing.\nPlease wait for disk activity to stop before removing disk.\n");
		}
	}
	free(zero);
	return(ret);
}

/*
loads an entire .fds file, and writes each side to disk, prompting for disk changes
*/
bool FDS_writeDisk(char *filename)
{
	const int DISKSIZE = 0x10000 + 8192;
	bin_t bins[16];			//plenty of room (bad practice i think this is)
	uint8_t *ptr, *buf = 0;
	int filesize, i, sides;
	bool ret = false;

	//load entire file into a buffer
	if (!loadfile(filename, &buf, &filesize)) {
		printf("Error loading %s\n", filename);
		return(false);
	}

	//clear the array
	memset(&bins, 0, sizeof(bin_t) * 16);

	//use pointer to access disk data
	ptr = buf;

	//detect fwnes header
	if (ptr[0] == 'F' && ptr[1] == 'D' && ptr[2] == 'S' && ptr[3] == 0x1A) {
		ptr += 16;
		printf("Skipping fwNES header.\n");
		filesize -= (filesize - 16) % FDSSIZE;  //truncate down to whole disk
	}

	//no header, just make sure the size is correct
	else {
		filesize -= filesize % FDSSIZE;  //truncate down to whole disk
	}

	//calculate number of disk sides
	sides = filesize / FDSSIZE;
	printf("FDS format detected. (filesize = %d, %d disk sides)\n", filesize, sides);

	//fill bins array with disk data
	for (i = 0; i < sides; i++) {

		//allocate buffer
		bins[i].data = (uint8_t*)malloc(DISKSIZE);
		memset(bins[i].data, 0, DISKSIZE);

		//convert disk to bin format
		bins[i].size = fds_to_bin(bins[i].data + LEAD_IN, ptr, DISKSIZE - LEAD_IN);

		//check if conversion was successful
		if (bins[i].size == 0) {
			break;
		}

		//add lead-in size to converted image size and advance input pointer
		bins[i].size += LEAD_IN;
		ptr += FDSSIZE;
	}

	//if the previous loop completed (i is equal to sides) then it processed the disk properly, now send to fdsemu for writing
	if (i == sides) {
		ret = BIN_writeDisk(bins, sides);
	}

	//free any used memory
	for (i = 0; i < sides; i++) {
		if (bins[i].data != 0) {
			free(bins[i].data);
		}
	}

	//free file data
	free(buf);

	//return
	return(ret);
}

//write supercard disks
bool SC_writeDisk(char *filename) {
	return(false);
}

bool find_doctors(char *first, char **files, int *numfiles, int maxfiles);

bool GD_writeDisk(char *filename)
{
	const int DISKSIZE = 0x10000 + 8192;
	bin_t bins[16];			//plenty of room (bad practice i think this is)
	char *files[16];
	int numfiles = 0;
	uint8_t *buf = 0;
	uint8_t *binbuf = 0;
	int i, len;
	bool ret = false;

	/*	uint8_t *ptr;
	int i, slot;
	char *shortName;
	uint8_t *buf, *cbuf;
	int len, clen;*/

	//clear the array
	memset(&bins, 0, sizeof(bin_t) * 16);

	//find files for this disk image
	if (find_doctors(filename, files, &numfiles, 16) == false) {
		printf("Failed to find doctor disk sides\n");
		return(false);
	}

	//allocate temporary space for storing the bins
	binbuf = (uint8_t*)malloc(DISKSIZE);

	//loop thru found files
	for (i = 0; i < numfiles; i++) {
		printf("Loading %s...", files[i]);

		//load game doctor image file
		if (loadfile(files[i], &buf, &len) == false) {
			printf("Error loading file\n");
			break;
		}

		/*
		better way to do this:
		--modify gameDoctor_to_bin to accept a max size of input files instead of continuing along a large chunk of data
		*/
		//copy it to other block of data
		memset(binbuf, 0, DISKSIZE);
		memcpy(binbuf, buf, len);

		//allocate buffer
		bins[i].data = (uint8_t*)malloc(DISKSIZE);
		memset(bins[i].data, 0, DISKSIZE);

		//convert disk to bin format
		bins[i].size = gameDoctor_to_bin(bins[i].data + LEAD_IN, binbuf, DISKSIZE - LEAD_IN);

		//check if conversion was successful
		if (bins[i].size == 0) {
			printf("Error converting gamedoctor disk image '%s' to bin\n", files[i]);
			break;
		}

		//account for lead-in data
		bins[i].size += LEAD_IN;

		free(buf);
		printf("\n");
	}

	//if the previous loop completed (i is equal to sides) then it processed the disk properly, now send to fdsemu for writing
	if (i == numfiles) {
		ret = BIN_writeDisk(bins, numfiles);
	}

	//free any used memory
	for (i = 0; i < numfiles; i++) {
		if (bins[i].data != 0) {
			free(bins[i].data);
		}
	}

	free(binbuf);

	return(ret);
}

bool writeDisk(char *filename)
{
	FILE *fp;
	uint8_t buf[128];
	bool ret = false;

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
			ret = FDS_writeDisk(filename);
		}

		else if (buf[0] == 0x01 && buf[1] == 0x2A && buf[2] == 0x4E && buf[0x38] == 0x02) {
			printf("Detected FDS format.\n");
			ret = FDS_writeDisk(filename);
		}

		//detect game doctor format
		else if (buf[3] == 0x01 || buf[4] == 0x2A || buf[5] == 0x4E || buf[0x3D] == 0x02) {
			printf("Detected Game Doctor format.\n");
			ret = GD_writeDisk(filename);
		}

		else {
			printf("unknown disk image format\n");
		}
	}
	fclose(fp);
	return(ret);
}

bool FDS_writeDisk_OLD(char *filename) {
	enum {
		LEAD_IN = DEFAULT_LEAD_IN / 8,
		DISKSIZE = 0x10000 + 8192,               //whole disk contents including lead-in
		ZEROSIZE = 0x10000 + 8192,						//maximum size of sram + "doctor" extra ram
	};
	uint8_t *inbuf = 0;       //.FDS buffer
	uint8_t *bin = 0;         //.FDS with gaps/CRC
	uint8_t *zero = 0;
	int filesize;
	int binSize;
	int format = 0;

	if (!loadfile(filename, &inbuf, &filesize))
	{
		printf("Can't read %s\n", filename); return false;
	}

	bin = (uint8_t*)malloc(DISKSIZE);
	zero = (uint8_t*)malloc(ZEROSIZE);
	memset(zero, 0, ZEROSIZE);

	int inpos = 0, side = 0;

	//detect game doctor header
	if (inbuf[3] == 0x01 && inbuf[4] == 0x2A && inbuf[5] == 0x4E && inbuf[6] == 0x49 && inbuf[61] == 0x02) {
		format = 1;
		printf("Game Doctor format detected. (file size = %d bytes)\n", filesize);
	}
	else {
		//detect fwnes header
		if (inbuf[0] == 'F' && inbuf[1] == 'D' && inbuf[2] == 'S' && inbuf[3] == 0x1A) {
			inpos = 16;      //skip fwNES header
			printf("Skipping fwNES header.\n");
		}
		format = 0;
		filesize -= (filesize - inpos) % FDSSIZE;  //truncate down to whole disk
		printf("FDS format detected.\n");
	}

	char prompt;
	do {
		printf("Side %d\n", side + 1);

		if (dev.Sram->Write(zero, 0, ZEROSIZE) == false) {
			printf("Sram write failed (zero).\n");
			return(false);
		}
		memset(bin, 0, LEAD_IN);
		if (format == 0) {
			binSize = fds_to_bin(bin + LEAD_IN, inbuf + inpos, DISKSIZE - LEAD_IN);
		}
		else if (format == 1) {
			binSize = gameDoctor_to_bin(bin + LEAD_IN, inbuf + inpos, DISKSIZE - LEAD_IN);
		}
		else {
			printf("Error converting format to BIN. (bug)\n");
			break;
		}
		if (!binSize)
			break;
		if (!writeDisk2(bin, binSize + LEAD_IN))
			break;
		inpos += FDSSIZE;
		side++;

		//		printf("finished write, inpos = %d, filesize = %d, inbuf[inpos] = %X", inpos, filesize, inbuf[inpos]);
		//prompt for disk change
		prompt = 0;
		if (inpos<filesize && inbuf[inpos] == 0x01) {
			printf("\nPlease wait for disk activity to stop before pressing ENTER to write the next disk side.\n");
			prompt = readKb();
		}
		else {
			printf("\nDisk image sent to SRAM on device and is currently writing.\nPlease wait for disk activity to stop before removing disk.\n");
		}
	} while (prompt == 0x0d);

	free(bin);
	free(zero);
	free(inbuf);
	return true;
}
