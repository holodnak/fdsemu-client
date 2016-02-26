#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "diskutil.h"
#include "flashrw.h"

void cli_progress(void *user, int bytes, int side)
{
	printf(".");
}

/*
decompress lz4 data.

buf = raw lz4 data, including 16 byte header
cb_read = callback for reading uncompressed data
cb_write = callback for writing uncompressed data
*/
int decompress_lz4(uint8_t *src, uint8_t *dest, int srclen, int destlen)
{
	uint8_t token, tmp;
	int inlen = 0;
	int outlen = 0;
	uint32_t offset;
	uint32_t n;

	inlen += 4;
	inlen += 7;

	//loop thru
	while (inlen < srclen) {
		token = src[inlen++];

		//literal part
		if ((token >> 4) & 0xF) {

			//calculate literal length
			n = (token >> 4) & 0xF;

			//length of 15 or greater
			if (n == 0xF) {
				do {
					tmp = src[inlen++];
					n += tmp;
				} while (tmp == 0xFF);
			}

			//write literals to output
			while (n--) {
				dest[outlen++] = src[inlen++];
			}
		}

		//match part (if it is there)
		if ((inlen + 12) >= srclen) {
			break;
		}

		//get match offset
		offset = src[inlen++];
		offset |= src[inlen++] << 8;

		//calculate match length
		n = token & 0xF;

		//length of 15 or greater
		if (n == 0xF) {
			do {
				tmp = src[inlen++];
				n += tmp;
			} while (tmp == 0xFF);
		}

		//add 4 to match length
		n += 4;
		offset = outlen - offset;

		//copy match bytes
		while (n--) {
			tmp = dest[offset++];
			dest[outlen++] = tmp;
		}
	}

	return(outlen);
}

bool read_flash(char *filename_fds, int slot)
{
	enum {
		RAWSIZE = SLOTSIZE * 8,
	};

	static uint8_t fwnesHdr[16] = { 0x46, 0x44, 0x53, 0x1a, };

	FILE *f;
	uint8_t *bin, *raw, *fds;
	bool result = true;

	f = fopen(filename_fds, "wb");
	if (!f) {
		printf("Can't create %s\n", filename_fds);
		return false;
	}

	printf("Reading disk image from flash slot %d to '%s'...\n", slot, filename_fds);
	fwnesHdr[4] = 0;
	fwrite(fwnesHdr, 1, sizeof(fwnesHdr), f);

	bin = (uint8_t*)malloc(SLOTSIZE + 8192);     //single side from flash
	raw = (uint8_t*)malloc(RAWSIZE);      //..to raw03
	fds = (uint8_t*)malloc(FDSSIZE + 0x10000 + 16);      //..to FDS

	int side = 0;
	for (; side + slot <= (int)dev.Slots; side++) {
		if (!dev.Flash->Read(bin, (slot + side)*SLOTSIZE, SLOTSIZE)) {
			result = false;
			break;
		}

		if (bin[0] == 0xff || (bin[0] != 0 && side != 0)) {    //stop on empty slot or next game
			break;
		}
		else if (bin[0] == 0 && side == 0) {
			printf("Warning! Not first side of game\n");
		}

		printf("Side %d\n", side + 1);
		/*
		flags: CRxx-xxTT

		C = compressed
		R = read-only
		T = type (0=standard fds, 1=game doctor)
		*/
		//check image flags
		if (bin[248] & 0x80) {
			int srclen = bin[240] | (bin[241] << 8);
			int destlen;
			char filename[512];
			FILE *fp;
			uint8_t *output = (uint8_t*)malloc(0x20000);

			memset(output, 0, 0x20000);
			destlen = decompress_lz4(bin + 256, output, srclen, 0x20000);
			printf("Decompressing image (%d -> %d)...\n",srclen,destlen);
			sprintf(filename, "%s.bin.%c", filename_fds, 'A' + side);
			fp = fopen(filename, "wb");
			fwrite(output, 1, destlen, fp);
			fclose(fp);
			free(output);
		}
		else {
			memset(bin, 0, FLASHHEADERSIZE);  //clear header, use it as lead-in
			bin_to_raw03(bin, raw, SLOTSIZE, RAWSIZE);
			if (!raw03_to_fds(raw, fds, RAWSIZE)) {
				result = false;
				break;
			}
			fwrite(fds, 1, FDSSIZE, f);
			fwnesHdr[4]++;  //count sides written
		}
	}

	fseek(f, 0, SEEK_SET);
	fwrite(fwnesHdr, 1, sizeof(fwnesHdr), f);      //update disk side count

	free(fds);
	free(raw);
	free(bin);
	fclose(f);
	return result;
}

bool read_flash_raw(char *filename, int slot)
{
	bool result = true;
	FILE *f;
	uint8_t *raw;

	f = fopen(filename, "wb");
	if (!f) {
		printf("Can't create %s\n", filename);
		return false;
	}

	printf("Reading raw data from flash slot %d to '%s'...\n", slot, filename);

	raw = (uint8_t*)malloc(0x10000);

	int side = 0;

	if (!dev.Flash->Read(raw, slot*SLOTSIZE, SLOTSIZE)) {
		printf("error reading from flash\n");
		result = false;
	}
	else {
		fwrite(raw, 1, SLOTSIZE, f);
	}

	free(raw);
	fclose(f);
	return result;
}

int find_slot(int slots)
{
	TFlashHeader *headers = dev.FlashUtil->GetHeaders();
	int i, j, slot = -1;

	for (i = 0; i < (int)dev.Slots; i++) {

		//check if slot is empty
		if (headers[i].filename[0] == 0xFF) {

			//check for more empty slots adjacent to this one
			for (j = 1; j < slots; j++) {
				if (headers[i + j].filename[0] != 0xFF) {
					break;
				}
			}

			//found an area sufficient to store this flash image
			if (j == slots) {
				slot = i;
				break;
			}
		}
	}
	return(slot);
}

bool write_flash(char *filename, int slot)
{
	enum { FILENAMELENGTH = 240, };   //number of characters including null

	uint8_t *inbuf = 0;
	uint8_t *outbuf = 0;
	uint8_t *verifybuf = 0;
	int filesize;
	uint32_t i;
	char *shortName;
	TFlashHeader *headers = dev.FlashUtil->GetHeaders();
	bool ret = true;

	if (headers == 0) {
		printf("error reading flash headers");
		return(false);
	}

	if (!loadfile(filename, &inbuf, &filesize))
	{
		printf("Can't read %s\n", filename); return false;
	}

	int pos = 0, side = 0;
	if (inbuf[0] == 'F')
		pos = 16;      //skip fwNES header

	filesize -= (filesize - pos) % FDSSIZE;  //truncate down to whole disks
											 //strip path from filename
	shortName = strrchr(filename, '/');      // ...dir/file.fds
#ifdef _WIN32
	if (!shortName)
		shortName = strrchr(filename, '\\');        // ...dir\file.fds
	if (!shortName)
		shortName = strchr(filename, ':');         // C:file.fds
#endif
	if (!shortName)
		shortName = filename;
	else
		shortName++;

	//check if an image of the same name is is already stored
/*	for (i = 0; i < dev.Slots; i++) {
		if (strncmp(shortName, (char*)headers[i].filename, 240) == 0) {
			if (force == 0) {
				printf("An image of the same name is already stored in flash.\n");
				printf("If you really want to store this, add '--force' to the command line.\n");
				delete[] inbuf;
				return(false);
			}
		}
	}*/

	//try to find an area to store the disk image
	if (slot == -1) {
		slot = find_slot(filesize / FDSSIZE);
	}

	if (slot == -1) {
		printf("Cannot find %d adjacent slots for storing disk image.\nPlease make room on the flash to store this disk image.\n", filesize / FDSSIZE);
		delete[] inbuf;
		return(false);
	}

	printf("Writing disk image to flash slot %d...\n", slot);

	outbuf = new uint8_t[SLOTSIZE];
	verifybuf = new uint8_t[SLOTSIZE];

	while (pos<filesize && inbuf[pos] == 0x01) {
		printf("Side %d", side + 1);
		if (fds_to_bin(outbuf + FLASHHEADERSIZE, inbuf + pos, SLOTSIZE - FLASHHEADERSIZE)) {
			memset(outbuf, 0, FLASHHEADERSIZE);
			uint32_t chksum = chksum_calc(outbuf + FLASHHEADERSIZE, SLOTSIZE - FLASHHEADERSIZE);
			outbuf[244] = DEFAULT_LEAD_IN & 0xff;
			outbuf[245] = DEFAULT_LEAD_IN / 256;
			outbuf[250] = 0;

			if (side == 0) {
				strncpy((char*)outbuf, shortName, 240);
			}
			if (dev.Flash->Write(outbuf, (slot + side)*SLOTSIZE, SLOTSIZE, cli_progress) == false) {
				printf("error.\n");
				ret = false;
				break;
			}
			if (dev.Flash->Read(verifybuf, (slot + side)*SLOTSIZE, SLOTSIZE) == false) {
				printf("verify error.\n");
				ret = false;
				break;
			}
			if (memcmp(verifybuf, outbuf, SLOTSIZE) != 0) {
				printf("verify failed.\n");
				ret = false;
				break;
			}
			printf("done.\n");
		}
		pos += FDSSIZE;
		side++;
	}
	delete[] inbuf;
	delete[] outbuf;
	delete[] verifybuf;
	printf("\n");
	return ret;
}

bool load_doctor_disk(uint8_t **buf, int *len, char *file)
{
	uint8_t *filebuf = 0;
	int n, filelen = 0;

	if (loadfile(file, &filebuf, &filelen) == false) {
		printf("error loading '%s'\n", file);
		return(false);
	}

	*len = filelen * 2;
	*buf = new uint8_t[*len];

	if ((n = gameDoctor_to_bin(*buf, filebuf, *len)) == 0) {
		printf("error converting GD to bin\n");
		delete[] filebuf;
		delete[] * buf;
		*buf = 0;
		*len = 0;
		return(false);
	}

	*len = n;

	delete[] filebuf;

	return(true);
}

bool load_disk(uint8_t **buf, int *len, char *file)
{
	uint8_t *filebuf = 0;
	int n, filelen = 0;
	uint8_t *ptr;

	if (loadfile(file, &filebuf, &filelen) == false) {
		printf("error loading '%s'\n", file);
		return(false);
	}

	*len = filelen * 2;
	*buf = new uint8_t[*len];

	ptr = filebuf;
	if (memcmp(ptr, "FDS\x1A", 4) == 0) {
		ptr += 16;
	}

	if ((n = fds_to_bin(*buf, ptr, *len)) == 0) {
		printf("error converting fds to bin\n");
		delete[] filebuf;
		delete[] * buf;
		*buf = 0;
		*len = 0;
		return(false);
	}

	*len = n;

	delete[] filebuf;

	//	printf("loaded fds format image, filelen = %d, loadedlen = %d\n", filelen, *len);

	return(true);
}

#include "lz4/lz4.h"       /* still required for legacy format */
#include "lz4/lz4hc.h"     /* still required for legacy format */
#include "lz4/lz4frame.h"

/*****************************
*  Constants
*****************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _8BITS 0xFF

#define MAGICNUMBER_SIZE    4
#define LZ4IO_MAGICNUMBER   0x184D2204
#define LZ4IO_SKIPPABLE0    0x184D2A50
#define LZ4IO_SKIPPABLEMASK 0xFFFFFFF0
#define LEGACY_MAGICNUMBER  0x184C2102

#define CACHELINE 64
#define LEGACY_BLOCKSIZE   (8 MB)
#define MIN_STREAM_BUFSIZE (192 KB)
#define LZ4IO_BLOCKSIZEID_DEFAULT 7

#define sizeT sizeof(size_t)
#define maskT (sizeT - 1)

static int LZ4IO_GetBlockSize_FromBlockId(int id) { return (1 << (8 + (2 * id))); }

int compress_lz4(uint8_t *src, int srcsize, uint8_t **dst, int *dstsize)
{
	unsigned long compressedfilesize = 0;
	const size_t blockSize = (size_t)LZ4IO_GetBlockSize_FromBlockId(LZ4IO_BLOCKSIZEID_DEFAULT);
	LZ4F_preferences_t prefs;

	memset(&prefs, 0, sizeof(prefs));

	/* File check */
	*dst = new uint8_t[srcsize * 2];
	*dstsize = 0;

	prefs.autoFlush = 1;
	prefs.compressionLevel = 9;
	prefs.frameInfo.blockMode = (LZ4F_blockMode_t)1;
	prefs.frameInfo.blockSizeID = (LZ4F_blockSizeID_t)LZ4IO_BLOCKSIZEID_DEFAULT;
	prefs.frameInfo.contentChecksumFlag = (LZ4F_contentChecksum_t)1;

	size_t cSize = LZ4F_compressFrame(*dst, srcsize * 2, src, srcsize, &prefs);
	if (LZ4F_isError(cSize)) {
		printf("Compression failed : %s", LZ4F_getErrorName(cSize));
		return(1);
	}

	compressedfilesize += cSize;
	*dstsize = compressedfilesize;

	//	printf("Compressed %u bytes into %u bytes ==> %.2f%%\n",srcsize, compressedfilesize, (double)compressedfilesize / srcsize * 100); 

	return 0;
}

void hexdump(char *desc, void *addr, int len)
{
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	if (desc != NULL)
		printf("%s:\r\n", desc);
	for (i = 0; i < len; i++) {
		if ((i % 16) == 0) {
			if (i != 0)
				printf("  %s\r\n", buff);
			printf("  %04x ", i);
		}
		printf(" %02x", pc[i]);
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}
	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}
	printf("  %s\r\n", buff);
}

char *get_shortname(char *filename)
{
	char *shortName = strrchr(filename, '/');      // ...dir/file.fds

#ifdef _WIN32
	if (!shortName)
		shortName = strrchr(filename, '\\');        // ...dir\file.fds
	if (!shortName)
		shortName = strchr(filename, ':');         // C:file.fds
#endif
	if (!shortName)
		shortName = filename;
	else
		shortName++;
	return(shortName);
}

bool find_doctors(char *first, char **files, int *numfiles, int maxfiles)
{
	char *str;
	int n = 0;

	*numfiles = 0;

	//first check if the first file exists
	if (file_exists(first) == false) {
		printf("first disk file doesnt exist: %s\n", first);
		return(false);
	}

	//add first file to the list
	files[n++] = strdup(first);

	//copy the first filename into temporary string
	str = strdup(first);

	//find more files
	for (; n < maxfiles;) {
		str[strlen(str) - 1]++;
		//		printf("checking if file '%s' exists...", str);
		if (file_exists(str) == false) {
			break;
		}
		files[n++] = strdup(str);
	}

	//check for save disk
	if (n < maxfiles) {
		str[strlen(str) - 1] = 'S';
		if (file_exists(str) == true) {
			files[n++] = strdup(str);
		}
	}

	*numfiles = n;
	return(true);
}

//TODO: fix memleaks here, and clean up this code
bool write_doctor(char *file)
{
	uint8_t *blocks;		//all blocks for all the images
	int blockslen;			//length of blocks
	uint8_t *ptr;
	int i, slot;
	char *shortName;
	char *files[17];
	int numfiles = 0;
	uint8_t *buf, *cbuf;
	int len, clen;

	if (find_doctors(file, files, &numfiles, 17) == false) {
		return(false);
	}

	blockslen = 0x10000 * numfiles;
	blocks = new uint8_t[blockslen];
	memset(blocks, 0, blockslen);
	ptr = blocks;

	for (i = 0; i < numfiles; i++) {
		printf("loading %s...", files[i]);
		if (load_doctor_disk(&buf, &len, files[i]) == false) {
			if (load_disk(&buf, &len, files[i]) == false) {
				printf("error loading disk image '%s'\n", files[i]);
				delete[] blocks;
				return(false);
			}
		}
		if (compress_lz4(buf, len, &cbuf, &clen) != 0) {
			printf("error compressing disk image\n");
			delete[] buf;
			delete[] blocks;
			return(false);
		}
		delete[] buf;
		//		printf("loaded and compressed ok (%d bytes -> %d bytes)\n", len, clen);
		if (clen >= (0x10000 - 256)) {
			printf("disk too big to store in flash\n");
			delete[] cbuf;
			delete[] blocks;
			return(false);
		}
		printf("ok (%d bytes -> %d bytes)\n", len, clen);
		buf = 0;
		len = 0;
		if (i == 0) {
			shortName = get_shortname(files[i]);
			strncpy((char*)ptr, shortName, 240);
		}

		//size
		ptr[240] = (uint8_t)(clen & 0xFF);
		ptr[241] = (uint8_t)(clen >> 8);

		//leadin
		ptr[242] = DEFAULT_LEAD_IN & 0xff;
		ptr[243] = DEFAULT_LEAD_IN / 256;

		//next disk id
		ptr[244] = 0 & 0xff;
		ptr[245] = 0 / 256;

		//save disk id
		ptr[246] = 0 & 0xff;
		ptr[247] = 0 / 256;

		//flags
		ptr[248] = 0xC1;		//compressed, read only, game doctor format
		memcpy(ptr + 256, cbuf, clen);
		delete[] cbuf;
		cbuf = 0;
		clen = 0;
		ptr += 0x10000;
	}

	if (numfiles == 0) {
		printf("Failed to find files to write\n");
		delete[] blocks;
		return(false);
	}

	slot = find_slot(numfiles);

	if (slot < 0) {
		printf("Error finding a slot for disk image\n");
		delete[] blocks;
		return(false);
	}

	printf("Writing %d disk images starting at flash slot %d...\n", numfiles, slot);
	ptr = blocks;

	for (i = 0; i < numfiles; i++) {
		printf("Disk %c", 'A' + i);
		//		hexdump("ptr", ptr, 260);
		if (dev.Flash->Write(ptr, (slot + i)*SLOTSIZE, SLOTSIZE, cli_progress) == false) {
			printf("error.\n");
			break;
		}
		printf("done.\n");
		ptr += 0x10000;
	}

	for (i = 0; i < numfiles; i++) {
		if (files[i]) {
			free(files[i]);
			files[i] = 0;
		}
	}
	delete[] blocks;

	return(true);
}

bool is_gamedoctor(char *filename)
{
	uint8_t *buf = 0;
	int len = 0;
	bool ret = false;

	//make sure it is valid filename
	if (file_exists(filename) == true) {

		//check file extension
		if (filename[strlen(filename) - 1] == 'A' || filename[strlen(filename) - 1] == 'a') {

			//load the file
			if (loadfile(filename, &buf, &len) == true) {

				//verify header
				if (buf[3] == 0x01 && buf[4] == 0x2a && buf[5] == 0x4e && buf[0x3d] == 0x02) {
					ret = true;
				}
				delete[] buf;
			}
		}
	}
	return(ret);
}
