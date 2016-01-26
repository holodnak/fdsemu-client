/*
auto disk search?  wrote disk to slot 0...ensure more security of the loader's slot
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "Device.h"
#include "Flash.h"
#include "System.h"
#include "DiskImage.h"
#include "build.h"
#include "crc32.h"

CDevice dev;

enum {
	ACTION_NONE = 0,
	ACTION_CONVERT,
	ACTION_LIST,
	ACTION_READFLASH,
	ACTION_READFLASHRAW,
	ACTION_WRITEFLASH,
	ACTION_WRITEDOCTORFLASH,
	ACTION_ERASEFLASH,
	ACTION_READDISK,
	ACTION_WRITEDISK,
	ACTION_UPDATEFIRMWARE,
	ACTION_UPDATEBOOTLOADER,
	ACTION_UPDATELOADER,
	ACTION_CHIPERASE,
	ACTION_SELFTEST,
	ACTION_VERIFY,
};

void cli_progress(void *user, uint32_t n)
{
	printf(".");
}

uint8_t raw_to_raw03_byte(uint8_t raw)
{
	if (raw < 0x40)
		return(3);
	else if (raw < 0x70)
		return(0);
	else if (raw < 0xA0)
		return(1);
	else if (raw < 0xD0)
		return(2);
	return(3);
}

//Turn raw data from adapter to pulse widths (0..3)
//Input capture clock is 6MHz.  At 96.4kHz (FDS bitrate), 1 bit ~= 62 clocks
//We could be clever about this and account for drive speed fluctuations, etc. but this will do for now
static void raw_to_raw03(uint8_t *raw, int rawSize) {
	for (int i = 0; i<rawSize; ++i) {
		raw[i] = raw_to_raw03_byte(raw[i]);
	}
}

//don't include gap end
uint16_t calc_crc(uint8_t *buf, int size) {
	uint32_t crc = 0x8000;
	int i;
	while (size--) {
		crc |= (*buf++) << 16;
		for (i = 0; i<8; i++) {
			if (crc & 1) crc ^= 0x10810;
			crc >>= 1;
		}
	}
	return crc;
}

void copy_block(uint8_t *dst, uint8_t *src, int size) {
	dst[0] = 0x80;
	memcpy(dst + 1, src, size);
	uint32_t crc = calc_crc(dst + 1, size + 2);
	dst[size + 1] = crc;
	dst[size + 2] = crc >> 8;
//	printf("copying blocks, size = %d, crc = %04X\n", size + 2, crc);
}

//Adds GAP + GAP end (0x80) + CRCs to .FDS image
//Returns size (0=error)
int fds_to_bin(uint8_t *dst, uint8_t *src, int dstSize) {
	int i = 0, o = 0;

	//check *NINTENDO-HVC* header
	if (src[0] != 0x01 || src[1] != 0x2a || src[2] != 0x4e) {
		printf("Not an FDS file.\n");
		return 0;
	}
	memset(dst, 0, dstSize);

	//block type 1
	copy_block(dst + o, src + i, 0x38);
	i += 0x38;
	o += 0x38 + 3 + GAP;

	//block type 2
	copy_block(dst + o, src + i, 2);
	i += 2;
	o += 2 + 3 + GAP;

	//block type 3+4...
	while (src[i] == 3) {
		int size = (src[i + 13] | (src[i + 14] << 8)) + 1;
		if (o + 16 + 3 + GAP + size + 3 > dstSize) {    //end + block3 + crc + gap + end + block4 + crc
			printf("Out of space (%d bytes short), adjust GAP size?\n", (o + 16 + 3 + GAP + size + 3) - dstSize);
			return 0;
		}
		copy_block(dst + o, src + i, 16);
		i += 16;
		o += 16 + 3 + GAP;

		copy_block(dst + o, src + i, size);
		i += size;
		o += size + 3 + GAP;
	}
	return o;
}

/*
Adds GAP + GAP end (0x80) + CRCs to Game Doctor image.  Returns size (0=error)

GD format:
0x??, 0x??, 0x8N      3rd byte seems to be # of files on disk, same as block 2.
repeat to end of disk {
N bytes (block contents, same as .fds)
2 dummy CRC bytes (0x00 0x00)
}
*/
int gameDoctor_to_bin(uint8_t *dst, uint8_t *src, int dstSize) {
	//check for *NINTENDO-HVC* at 0x03 and second block following CRC
	if (src[3] != 0x01 || src[4] != 0x2a || src[5] != 0x4e || src[0x3d] != 0x02) {
		printf("Not GD format.\n");
		return 0;
	}
	memset(dst, 0, dstSize);

	//block type 1
	int i = 3, o = 0;
	copy_block(dst + o, src + i, 0x38);
	i += 0x38 + 2;        //block + dummy crc
	o += 0x38 + 3 + GAP;    //gap end + block + crc + gap

									//block type 2
	copy_block(dst + o, src + i, 2);
	i += 2 + 2;
	o += 2 + 3 + GAP;

	//block type 3+4...
	while (src[i] == 3) {
		int size = (src[i + 13] | (src[i + 14] << 8)) + 1;
		if (o + 16 + 3 + GAP + size + 3 > dstSize) {    //end + block3 + crc + gap + end + block4 + crc
			printf("Out of space (%d bytes short), adjust GAP size?\n", (o + 16 + 3 + GAP + size + 3) - dstSize);
			return 0;
		}
		copy_block(dst + o, src + i, 16);
		i += 16 + 2;
		o += 16 + 3 + GAP;

		copy_block(dst + o, src + i, size);
		i += size + 2;
		o += size + 3 + GAP;
//		printf("copying blocks 3+4, size3 = %d, size4 = %d\n", 16 + 3, size + 3);
	}
	return o;
}

int action = ACTION_NONE;
int verbose = 0;
int force = 0;

//allocate buffer and read whole file
bool loadfile(char *filename, uint8_t **buf, int *filesize)
{
	FILE *fp;
	int size;
	bool result = false;

	//check if the pointers are ok
	if (buf == 0 || filesize == 0) {
		return(false);
	}

	//open file
	if ((fp = fopen(filename, "rb")) == 0) {
		return(false);
	}

	//get file size
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//allocate buffer
	*buf = new uint8_t[size];

	//read in file
	*filesize = fread(*buf, 1, size, fp);

	//close file and return
	fclose(fp);
	return(true);
}

static void usage(char *argv0)
{
	char usagestr[] =
		"  Flash operations:\n"
		"    -f file.fds [1..n]            write image to flash, optional to specify slot\n"
		"    -F file.A                     write doctor images to flash (only specify first disk file)\n"
		"    -s file.fds [1..n]            save image from flash slot [1..n]\n"
		"    -e [1..n] | all               erase slot [1..n] or erase all slots\n"
		"    -l                            list images stored on flash\n"
		"\n"
		"  Disk operations:\n"
		"    -R file.fds                   read disk to fwNES format disk image\n"
		"    -r file.raw [file.bin]        read disk to raw file (and optional bin file)\n"
		"    -w file.fds                   write disk from fwNES format disk image\n"
		"\n"
		"  Other operations:\n"
		"    -U firmware.bin               update firmware from firmware image\n"
		"\n"
		"  Options:\n"
		"    -v                            more verbose output\n"
		"";
/*	char usagestr[] =
		"Commands:\n\n"
		" Flash operations:\n\n"
		"  -f, --write-flash [files...]     write image(s) to flash\n"
		"  -F, --write-doctor [files...]    write doctor disk image sides to flash\n"
		"  -s, --save-flash [file] [1..n]   save image from flash slot [1..n]\n"
		"  -e, --erase-slot [1..n]          erase flash disk image in slot [1..n]\n"
		"  -d, --dump [file] [addr] [size]  dump flash from starting addr, size bytes\n"
		"  -W, --write-dump [file] [addr]   write raw flash data to flash\n"
		"\n"
		" Disk operations:\n\n"
		"  -r [file]                        read disk to file, type must be specified\n"
		"  -w [file]                        write file to disk\n"
		"\n"
		" Conversion operations:\n\n"
		"  -c, --convert [infile] [outfile] convert disk image, type must be specified\n"
		"\n"
		" Update operations:\n\n"
		"  -u, --update-loader [file]       update loader from fwFDS loader image\n"
		"  -U, --update-firmware [file]     update firmware from binary image\n"
		"\n"
		"Options:\n\n"
		"  -v, --verbose                    more verbose output\n"
		"      --force                      force operation\n"
		"  -t, --type [type]                specify file type, valid types: fds bin raw\n"
		"\n";*/

	printf("\n  usage: %s <options> <file(s)>\n\n%s", "fdsemu-cli", usagestr);
}

bool upload_firmware(uint8_t *firmware, int filesize)
{
	uint8_t *buf;
	uint32_t *buf32;
	uint32_t chksum;
	int i;

	//create new buffer to hold 32kb of data and clear it
	buf = new uint8_t[0x8000];
	memset(buf, 0, 0x8000);

	//copy firmware loaded to the new buffer
	memcpy(buf, firmware, filesize);
	buf32 = (uint32_t*)buf;

	//insert firmware identifier
	buf32[(0x8000 - 8) / 4] = 0xDEADBEEF;

	//calculate the simple xor checksum
	chksum = 0;
	for (i = 0; i < (0x8000 - 4); i += 4) {
		chksum ^= buf32[i / 4];
	}

	printf("firmware is %d bytes, checksum is $%08X\n", filesize, chksum);

	//insert checksum into the image
	buf32[(0x8000 - 4) / 4] = chksum;

	//newer firmwares store the firmware image in sram to be updated
	if (dev.Version > 792) {
		printf("uploading new firmware to sram\n");
		if (!dev.Sram->Write(buf, 0x0000, 0x8000)) {
			printf("Write failed.\n");
			return false;
		}
	}

	//older firmware store the firmware image into flash memory
	else {
		printf("uploading new firmware to flash");
		if (!dev.Flash->Write(buf, 0x8000, 0x8000, cli_progress)) {
			printf("Write failed.\n");
			return false;
		}
		printf("\n");
	}
	delete[] buf;

	printf("waiting for device to reboot\n");

	dev.UpdateFirmware();
	sleep_ms(5000);

	if (!dev.Open()) {
		printf("Open failed.\n");
		return false;
	}
	printf("Updated to build %d\n", dev.Version);
	return(true);
}

bool upload_bootloader(uint8_t *firmware, int filesize)
{
	uint8_t *buf;
	uint32_t *buf32;
	uint32_t oldcrc,crc;

	if (filesize > 0x1000) {
		printf("bootloader too large\n");
		return(false);
	}

	//get old crc
	oldcrc = dev.VerifyBootloader();

	//create new buffer to hold 4kb + 8 of data and clear it
	buf = new uint8_t[0x1000 + 8];
	memset(buf, 0, 0x1000 + 8);

	//copy firmware loaded to the new buffer
	memcpy(buf, firmware, filesize);
	buf32 = (uint32_t*)buf;

	//insert firmware identifier
	buf32[(0x1000) / 4] = 0xCAFEBABE;

	//calculate the crc32 checksum
	crc = crc32(buf, 0x1000 + 4);

	printf("bootloader is %d bytes, checksum is $%08X\n", filesize, crc);

	//insert checksum into the image
	buf32[(0x1000 + 4) / 4] = crc;

	printf("uploading new bootloader to sram\n");
	if (!dev.Sram->Write(buf, 0x0000, 0x1000 + 8)) {
		printf("Write failed.\n");
		return false;
	}

	delete[] buf;

	dev.UpdateBootloader();
	
/*	printf("waiting for device to reboot\n");

	dev.UpdateFirmware();
	sleep_ms(3000);

	if (!dev.Open()) {
		printf("Open failed.\n");
		return false;
	}*/

	printf("Updated bootloader, old crc = %08X, new crc = %08X\n", oldcrc, crc);
	return(true);
}

bool firmware_update(char *filename)
{
	uint8_t *firmware;
	int filesize;
	bool ret = false;

	//try to load the firmware image
	if (loadfile(filename, &firmware, &filesize) == false) {
		printf("Error loading firmware file %s'\n", filename);
		return(false);
	}

	ret = upload_firmware(firmware, filesize);

	delete[] firmware;
	return(ret);
}

bool bootloader_update(char *filename)
{
	uint8_t *bootloader;
	int filesize;
	bool ret = false;

	//try to load the firmware image
	if (loadfile(filename, &bootloader, &filesize) == false) {
		printf("Error loading bootloader file %s'\n", filename);
		return(false);
	}

	ret = upload_bootloader(bootloader, filesize);

	delete[] bootloader;
	return(ret);
}

uint32_t chksum_calc(uint8_t *buf, int size)
{
	uint32_t ret = 0;
	uint32_t *data = (uint32_t*)buf;
	int i;

	for (i = 0; i < size / 4; i++) {
		ret ^= buf[i];
	}
	return(ret);
}

//look for pattern of bits matching block 1
static int findFirstBlock(uint8_t *raw) {
	static const uint8_t dat[] = { 1,0,1,0,0,0,0,0, 0,1,2,2,1,0,1,0, 0,1,1,2,1,1,1,1, 1,1,0,0,1,1,1,0 };
	int i, len;
	for (i = 0, len = 0; i<0x2000 * 8; i++) {
		if (raw[i] == dat[len]) {
			if (len == sizeof(dat) - 1)
				return i - len;
			len++;
		}
		else {
			i -= len;
			len = 0;
		}
	}
	return -1;
}

bool block_decode(uint8_t *dst, uint8_t *src, int *inP, int *outP, int srcSize, int dstSize, int blockSize, char blockType) {
	if (*outP + blockSize + 2 > dstSize) {
		printf("Out of space\n");
		return false;
	}

	int in = *inP;
	int outEnd = (*outP + blockSize + 2) * 8;
	int out = (*outP) * 8;
	int start;

	//scan for gap end
	for (int zeros = 0; src[in] != 1 || zeros<MIN_GAP_SIZE; in++) {
		if (src[in] == 0) {
			zeros++;
		}
		else {
			zeros = 0;
		}
		if (in >= srcSize - 2)
			return false;
	}
	start = in;

	char bitval = 1;
	in++;
	do {
		if (in >= srcSize) {   //not necessarily an error, probably garbage at end of disk
									  //printf("Disk end\n"); 
			return false;
		}
		switch (src[in] | (bitval << 4)) {
		case 0x11:
			out++;
		case 0x00:
			out++;
			bitval = 0;
			break;
		case 0x12:
			out++;
		case 0x01:
		case 0x10:
			dst[out / 8] |= 1 << (out & 7);
			out++;
			bitval = 1;
			break;
		default: //Unexpected value.  Keep going, we'll probably get a CRC warning
					//printf("glitch(%d) @ %X(%X.%d)\n", src[in], in, out/8, out%8);
			out++;
			bitval = 0;
			break;
		}
		in++;
	} while (out<outEnd);
	if (dst[*outP] != blockType) {
		printf("Wrong block type %X(%X)-%X(%X) (found %d, expected %d)\n", start, *outP, in, out - 1, dst[*outP], blockType);
		return false;
	}
	out = out / 8 - 2;

	//printf("Out%d %X(%X)-%X(%X)\n", blockType, start, *outP, in, out-1);

	if (calc_crc(dst + *outP, blockSize + 2)) {
		uint16_t crc1 = (dst[out + 1] << 8) | dst[out];
		dst[out] = 0;
		dst[out + 1] = 0;
		uint16_t crc2 = calc_crc(dst + *outP, blockSize + 2);
		printf("Bad CRC (%04X!=%04X)\n", crc1, crc2);
	}

	dst[out] = 0;     //clear CRC
	dst[out + 1] = 0;
	dst[out + 2] = 0;   //+spare bit
	*inP = in;
	*outP = out;
	return true;
}

//Simplified disk decoding.  This assumes disk will follow standard FDS file structure
static bool raw03_to_fds(uint8_t *raw, uint8_t *fds, int rawsize) {
	int in, out;
	int dstsize = FDSSIZE + 0x10000;

	memset(fds, 0, dstsize);

	//lead-in can vary a lot depending on drive, scan for first block to get our bearings
	in = findFirstBlock(raw) - MIN_GAP_SIZE;
	if (in<0)
		return false;

	out = 0;
	if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 0x38, 1))
		return false;
	if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 2, 2))
		return false;
	do {
		if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 16, 3))
			return true;
		if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 1 + (fds[out - 16 + 13] | (fds[out - 16 + 14] << 8)), 4))
			return true;
	} while (in<rawsize);
	return true;
}

//make raw0-3 from flash image (sans header)
static void bin_to_raw03(uint8_t *bin, uint8_t *raw, int binSize, int rawSize) {
	int in, out;
	uint8_t bit, data;

	memset(raw, 0xff, rawSize);
	for (bit = 1, out = 0, in = 0; in<binSize * 8; in++) {
		if ((in & 7) == 0) {
			data = *bin;
			bin++;
		}
		bit = (bit << 7) | (1 & (data >> (in & 7)));   //LSB first
																	  //     bit = (bit<<7) | (1 & (bin[in/8]>>(in%8)));   //LSB first
		switch (bit) {
		case 0x00:  //10 10
			out++;
			raw[out]++;
			break;
		case 0x01:  //10 01
		case 0x81:  //01 01
			raw[out]++;
			out++;
			break;
		case 0x80:  //01 10
			raw[out] += 2;
			break;
		}
	}
	memset(raw + out, 3, rawSize - out);  //fill remainder with (undefined)
}

//check for gap at EOF
bool looks_like_file_end(uint8_t *raw, int start, int rawSize) {
	enum {
		MIN_GAP = 976 - 100,
		MAX_GAP = 976 + 100,
	};
	int zeros = 0;
	int in = start;
	for (; in<start + MAX_GAP && in<rawSize; in++) {
		if (raw[in] == 1 && zeros>MIN_GAP) {
			return true;
		}
		else if (raw[in] == 0) {
			zeros++;
		}
		if (raw[in] != 0)
			zeros = 0;
	}
	return in >= rawSize;  //end of disk = end of file!
}

//detect EOF by looking for good CRC.  in=start of file
//returns 0 if nothing found
int crc_detect(uint8_t *raw, int in, int rawSize) {
	static uint32_t crc;
	static uint8_t bitval;
	static int out;
	static bool match;

	//local function ;)
	struct {
		void shift(uint8_t bit) {
			crc |= bit << 16;
			if (crc & 1) crc ^= 0x10810;
			crc >>= 1;
			bitval = bit;
			out++;
			if (crc == 0 && !(out & 7))  //on a byte bounary and CRC is valid
				match = true;
		}
	} f;

	crc = 0x8000;
	bitval = 1;
	out = 0;
	do {
		match = false;
		switch (raw[in] | (bitval << 4)) {
		case 0x11:
			f.shift(0);
		case 0x00:
			f.shift(0);
			break;
		case 0x12:
			f.shift(0);
		case 0x01:
		case 0x10:
			f.shift(1);
			break;
		default:    //garbage / bad encoding
			return 0;
		}
		in++;
	} while (in<rawSize && !(match && looks_like_file_end(raw, in, rawSize)));
	return match ? in : 0;
}

//gap end is known, backtrack and mark the start.  !! this assumes junk data exists between EOF and gap start
static void mark_gap_start(uint8_t *raw, int gapEnd) {
	int i;
	for (i = gapEnd - 1; i >= 0 && raw[i] == 0; --i)
	{
	}
	raw[i + 1] = 3;
	printf("mark gap %X-%X\n", i + 1, gapEnd);
}

//For information only for now.  This checks for standard file format
static void verify_block(uint8_t *bin, int start, int *reverse) {
	enum { MAX_GAP = (976 + 100) / 8, MIN_GAP = (976 - 100) / 8 };
	static const uint8_t next[] = { 0,2,3,4,3 };
	static int last = 0;
	static int lastLen = 0;
	static int blockCount = 0;

	int len = 0;
	uint8_t type = bin[start];

	printf("%d:%X", ++blockCount, type);

	switch (type) {
	case 1:
		len = 0x38;
		break;
	case 2:
		len = 2;
		break;
	case 3:
		len = 16;
		break;
	case 4:
		len = 1 + (bin[last + 13] | (bin[last + 14] << 8));
		break;
	default:
		printf(" bad block (%X)\n", start);
		return;
	}
	printf(" %X-%X / %X-%X(%X)", reverse[start], reverse[start + len], start, start + len, len);

	if ((!last && type != 1) || (last && type != next[bin[last]]))
		printf(", wrong filetype");
	if (calc_crc(bin + start, len + 2) != 0)
		printf(", bad CRC");
	if (last && (last + lastLen + MAX_GAP)<start)
		printf(", lost block?");
	if (last + lastLen + MIN_GAP>start)
		printf(", block overlap?");
	//if(type==3 && ...)    //check other fields in file header?

	printf("\n");
	last = start;
	lastLen = len;
}

//find gap + gap end.  returns bit following gap end, >=rawSize if not found.
int nextGapEnd(uint8_t *raw, int in, int rawSize) {
	enum { MIN_GAP = 976 - 100, };
	int zeros = 0;
	for (; (raw[in] != 1 || zeros<MIN_GAP) && in<rawSize; in++) {
		if (raw[in] == 0) {
			zeros++;
		}
		else {
			zeros = 0;
		}
	}
	return in + 1;
}


/*
Try to create byte-for-byte, unadulterated representation of disk.  Use hints from the disk structure, given
that it's probably a standard FDS game image but this should still make a best attempt regardless of the disk content.

_bin and _binSize are updated on exit.  alloc'd buffer is returned in _bin, caller is responsible for freeing it.
*/
static void raw03_to_bin(uint8_t *raw, int rawSize, uint8_t **_bin, int *_binSize) {
	enum {
		BINSIZE = 0xa0000,
		POST_GLITCH_GARBAGE = 16,
		LONG_POST_GLITCH_GARBAGE = 64,
		LONG_GAP = 900,   //976 typ.
		SHORT_GAP = 16,
	};
	int in, out;
	uint8_t *bin;
	int *reverse;
	int glitch;
	int zeros;

	bin = (uint8_t*)malloc(BINSIZE);
	reverse = (int*)malloc(BINSIZE*sizeof(int));
	memset(bin, 0, BINSIZE);

	//--- assume any glitch is OOB, mark a run of zeros near a glitch as a gap start.

	int junk = 0;
	glitch = 0;
	zeros = 0;
	junk = 0;
	for (in = 0; in<rawSize; in++) {
		if (raw[in] == 3) {
			glitch = in;
			junk = 0;
		}
		else if (raw[in] == 1 || raw[in] == 2) {
			junk = in;
		}
		else if (raw[in] == 0) {
			zeros++;
			if (glitch && junk && zeros>SHORT_GAP && (junk - glitch)<POST_GLITCH_GARBAGE) {
				mark_gap_start(raw, in);
				glitch = 0;
			}
		}
		if (raw[in] != 0)
			zeros = 0;
	}

	//--- Walk filesystem, mark blocks where something looks like a valid file

	in = findFirstBlock(raw);
	if (in>0) {
		printf("header at %X\n", in);
		mark_gap_start(raw, in - 1);
	}
	/*
	do {
	if(block_decode(..)) {
	raw[head]=0xff;
	raw[tail]=3;
	}
	next_gap(..);
	} while(..);
	*/
	//--- Identify files by CRC. If data looks like it's surrounded by gaps and it has a valid CRC where we
	//    expect one to be, assume it's a file and mark its start/end.

	in = findFirstBlock(raw) + 1;
	if (in>0) do {
		out = crc_detect(raw, in, rawSize);
		if (out) {
			printf("crc found %X-%X\n", in, out);
			raw[out] = 3;     //mark glitch (gap start)
									//raw[in-1]=0xff;   //mark gap end 
		}
		in = nextGapEnd(raw, out ? out : in, rawSize);
	} while (in<rawSize);

	//--- mark gap start/end using glitches to find gap start

	for (glitch = 0, zeros = 0, in = 0; in<rawSize; in++) {
		if (raw[in] == 3) {
			glitch = in;
		}
		else if (raw[in] == 1) {
			if (zeros>LONG_GAP && (in - zeros - LONG_POST_GLITCH_GARBAGE)<glitch) {
				mark_gap_start(raw, in);
				raw[in] = 0xff;
			}
		}
		else if (raw[in] == 0) {
			zeros++;
		}
		if (raw[in] != 0)
			zeros = 0;
	}

	//--- output

	/*
	FILE *f=fopen("raw03.bin","wb");
	fwrite(raw,1,rawSize,f);
	fclose(f);
	*/

	char bitval = 0;
	int lastBlockStart = 0;
	for (in = 0, out = 0; in<rawSize; in++) {
		switch (raw[in] | (bitval << 4)) {
		case 0x11:
			out++;
		case 0x00:
			out++;
			bitval = 0;
			break;
		case 0x12:
			out++;
		case 0x01:
		case 0x10:
			bin[out / 8] |= 1 << (out & 7);
			out++;
			bitval = 1;
			break;
		case 0xff:  //block end
			if (lastBlockStart)
				verify_block(bin, lastBlockStart, reverse);
			bin[out / 8] = 0x80;
			out = (out | 7) + 1;      //byte-align for readability
			lastBlockStart = out / 8;
			bitval = 1;
			break;
		case 0x02:
			//printf("Encoding error @ %X(%X)\n",in,out/8);
		default: //anything else (glitch)
			out++;
			bitval = 0;
			break;
		}
		reverse[out / 8] = in;
	}
	//last block
	verify_block(bin, lastBlockStart, reverse);

	*_bin = bin;
	*_binSize = out / 8 + 1;
	free(reverse);
}

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
		int binSize;

		raw03_to_bin(readBuf, bytesIn, &binBuf, &binSize);
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

bool FDS_writeDisk(char *filename) {
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
		printf("Game Doctor format detected. (file size = %d bytes)\n",filesize);
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

	bin = (uint8_t*)malloc(SLOTSIZE);     //single side from flash
	raw = (uint8_t*)malloc(RAWSIZE);      //..to raw03
	fds = (uint8_t*)malloc(FDSSIZE + 0x10000 + 16);      //..to FDS

	int side = 0;
	for (; side + slot <= (int)dev.Slots; side++) {
		if (!dev.Flash->Read(bin,(slot + side)*SLOTSIZE, SLOTSIZE)) {
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
		memset(bin, 0, FLASHHEADERSIZE);  //clear header, use it as lead-in
		bin_to_raw03(bin, raw, SLOTSIZE, RAWSIZE);
		if (!raw03_to_fds(raw, fds, RAWSIZE)) {
			result = false;
			break;
		}
		fwrite(fds, 1, FDSSIZE, f);
		fwnesHdr[4]++;  //count sides written
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
	int filesize;
	uint32_t i;
	char *shortName;
	TFlashHeader *headers = dev.FlashUtil->GetHeaders();

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
	for (i = 0; i < dev.Slots; i++) {
		if (strncmp(shortName, (char*)headers[i].filename, 240) == 0) {
			if (force == 0) {
				printf("An image of the same name is already stored in flash.\n");
				printf("If you really want to store this, add '--force' to the command line.\n");
				delete[] inbuf;
				return(false);
			}
		}
	}

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
				break;
			}
			printf("done.\n");
		}
		pos += FDSSIZE;
		side++;
	}
	delete[] inbuf;
	delete[] outbuf;
	printf("\n");
	return true;
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

#include "lz4.h"       /* still required for legacy format */
#include "lz4hc.h"     /* still required for legacy format */
#include "lz4frame.h"

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

static int compress_lz4(uint8_t *src, int srcsize, uint8_t **dst, int *dstsize)
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

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#ifndef F_OK
#define F_OK 00
#define R_OK 04
#endif
#endif

bool file_exists(char *fn)
{
	if (access(fn, F_OK) != -1) {
		return(true);
	}
	return(false);
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
	char *files[16];
	int numfiles = 0;
	uint8_t *buf, *cbuf;
	int len, clen;

	if (find_doctors(file, files, &numfiles, 16) == false) {
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
		ptr[240] = (uint8_t)(clen & 0xFF);
		ptr[241] = (uint8_t)(clen >> 8);
		ptr[242] = DEFAULT_LEAD_IN & 0xff;
		ptr[243] = DEFAULT_LEAD_IN / 256;
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

bool fds_list(int verbose)
{
	TFlashHeader *headers = dev.FlashUtil->GetHeaders();
	uint32_t i;
	int side = 0, empty = 0;

	if (headers == 0) {
		return(false);
	}

	printf("Listing disks stored in flash:\n");

	for (i=0; i < dev.Slots; i++) {
		uint8_t *buf = headers[i].filename;

		//verbose listing
		if (verbose) {
			if (buf[0] == 0xFF) {          //empty
				printf("%d:\n", i);
				side = 0;
				empty++;
			}
			else if (buf[0] != 0) {      //filename present
				printf("%d: %s\n", i, buf);
				side = 1;
			}
			else if (!side) {          //first side is missing
				printf("%d: ?\n", i);
			}
			else {                    //next side
				printf("%d:    Side %d\n", i, ++side);
			}
		}

		//short listing
		else {
			if (buf[0] == 0xFF) {          //empty
				empty++;
			}

			//filename is here
			else if (buf[0] != 0) {
				printf("%d: %s\n", i, buf);
			}
		}
	}
	printf("\nEmpty slots: %d\n", empty);
	return(true);
}

bool convert_image(char *infile, char *outfile, char *type)
{
//	CDiskImage image;

//	image.Load(infile);
	return(true);
}

bool chip_erase()
{
	uint8_t cmd[] = { CMD_CHIPERASE,0,0,0 };

	if (!dev.Flash->WriteEnable())
		return false;
	if (!dev.FlashWrite(cmd, 1, 1, 0))
		return false;
	return(dev.Flash->WaitBusy(200 * 1000));
}

bool verify_device()
{
	uint8_t *buf, *buf2;
	int i;

	bool ok = false;

	buf = new uint8_t[0x10000];
	buf2 = new uint8_t[0x10000];

	do {
		printf("Testing flash read/write...");

		//read data first (to preserve data)
		dev.Flash->Read(buf, 0, 0x10000);

		//generate simple test pattern
		for (i = 0; i < 0x10000; i++) {
			buf2[i] = (uint8_t)i;
		}

		//write simple test pattern, then read it back
		dev.Flash->Write(buf2, 0, 0x10000);
		dev.Flash->Read(buf2, 0, 0x10000);

		//verify flash data
		for (i = 0; i < 0x10000; i++) {
			if (buf2[i] != (uint8_t)i) {
				printf("error\n");
				break;
			}
		}

		//write old data back
		dev.Flash->Write(buf, 0, 0x10000);
		printf("ok\n");

		printf("Testing sram read/write...");
		//write test pattern to sram, then read it back
		dev.Sram->Write(buf2, 0, 0x10000);
		dev.Sram->Read(buf2, 0, 0x10000);

		//verify sram data
		for (i = 0; i < 0x10000; i++) {
			if (buf2[i] != (uint8_t)i) {
				printf("error\n");
				break;
			}
		}
		printf("ok\n");

		printf("Asking device to do a self-test...\n");
		ok = dev.Selftest();
		printf("If FDSemu LED indicator is RED, the self-test has failed.\n");

	} while (0);

	delete[] buf;
	delete[] buf2;
	return(ok);
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

int detect_firmware_build(uint8_t *fw, int len)
{
	const char ident[] = "NUC123-FDSemu Firmware by James Holodnak";
	int identlen = strlen(ident);
	uint8_t byte, *ptr = fw;
	int i;
	int ret = -1;

	for (i = 0; i < (len - identlen); i++, fw++) {
		
		//first byte is a match, continue checking
		if (*fw == (uint8_t)ident[0]) {

			//check for match
			if (memcmp(fw, ident, identlen) == 0) {
				ret = *(fw + identlen);
				ret |= *(fw + identlen + 1) << 8;
				break;
			}
		}
	}
	return(ret);
}

bool erase_slot(int slot)
{
	TFlashHeader *headers = dev.FlashUtil->GetHeaders();
	uint8_t *buf;

	if (dev.Flash->Erase(slot * SLOTSIZE, SLOTSIZE) == false) {
		return(false);
	}
	for (uint32_t i = (slot + 1); i<dev.Slots; i++) {
		buf = headers[i].filename;
		if (buf[0] == 0xff) {          //empty
			break;
		}
		else if (buf[0] != 0) {      //filename present
			break;
		}
		else {                    //next side
			if (dev.Flash->Erase(i * SLOTSIZE, SLOTSIZE) == false) {
				return(false);
			}
			printf(", %d", i);
		}
	}
	return(true);
}

extern unsigned char firmware[];
extern int firmware_length;

int main(int argc, char *argv[])
{
	int i, slot;
	bool success = false;
	char *param = 0;
	char *param2 = 0;
	char *param3 = 0;
	char *params[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	int numparams = 0;
	int required_build = -1;

	crc32_gentab();

	printf("fdsemu-cli v%d.%d.%d by James Holodnak, based on code by loopy\n", VERSION / 100, VERSION % 100, BUILDNUM);

	required_build = detect_firmware_build((uint8_t*)firmware, firmware_length);

	if (argc < 2) {
		usage(argv[0]);
		return(1);
	}

	//parse command line
	for (i = 1; i < argc; i++) {

		//get program usage
		if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return(1);
		}

		//use more verbose output
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
			verbose = 1;
		}

		//force the operation, whatever it may be
		else if (/*strcmp(argv[i], "-f") == 0 ||*/ strcmp(argv[i], "--force") == 0) {
			force = 1;
		}

		//erase entire chip
		else if (/*strcmp(argv[i], "-f") == 0 ||*/ strcmp(argv[i], "--chip-erase") == 0) {
			action = ACTION_CHIPERASE;
		}

		//list disk images
		else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
			action = ACTION_LIST;
		}

		//read disk image to disk
		else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--read-disk") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename.\n");
				return(1);
			}
			action = ACTION_READDISK;
			param = argv[++i];
			if (argv[i] && argv[i][0] != '-') {
				param2 = argv[++i];
			}
		}

		//read disk image to disk
		else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--read-disk-fds") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename.\n");
				return(1);
			}
			action = ACTION_READDISK;
			param3 = argv[++i];
		}

		//write disk image to disk
		else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--write-disk") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename.\n");
				return(1);
			}
			action = ACTION_WRITEDISK;
			param = argv[++i];
		}

		//write disk image to flash
		else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--write-flash") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename.\n");
				return(1);
			}
			action = ACTION_WRITEFLASH;
			param = argv[++i];
			if (argv[i] && argv[i][0] != '-') {
				param2 = argv[++i];
			}
		}

		//write doctor disk image to flash
		else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--write-doctor") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename.\n");
				return(1);
			}
			action = ACTION_WRITEDOCTORFLASH;
			param = argv[++i];
		}

		//read disk image from flash
		else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--save-flash") == 0) {
			if ((i + 2) >= argc) {
				printf("\nPlease specify a filename and slot number.\n");
				return(1);
			}
			action = ACTION_READFLASH;
			param = argv[++i];
			if (argv[i][0] != '-') {
				param2 = argv[++i];
			}
		}

		//read disk image from flash
		else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--save-flash-raw") == 0) {
			if ((i + 2) >= argc) {
				printf("\nPlease specify a filename and slot number.\n");
				return(1);
			}
			action = ACTION_READFLASHRAW;
			param = argv[++i];
			if (argv[i][0] != '-') {
				param2 = argv[++i];
			}
		}

		//erase disk image from flash
		else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--erase-flash") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a slot number (use --list --verbose to see flash contents).\n");
				return(1);
			}
			action = ACTION_ERASEFLASH;
			while ((i + 1) < argc) {
				params[numparams++] = argv[++i];
			}
		}

		//verify that the loader is in place and sram/flash is ok
		else if (strcmp(argv[i], "--verify") == 0) {
			action = ACTION_VERIFY;
		}

		//update loader
		else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--update-loader") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename for the loader to update with.\n");
				return(1);
			}
			action = ACTION_UPDATELOADER;
			param = argv[++i];
		}

		//update firmware
		else if (strcmp(argv[i], "-U") == 0 || strcmp(argv[i], "--update-firmware") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename for the firmware to update with.\n");
				return(1);
			}
			action = ACTION_UPDATEFIRMWARE;
			param = argv[++i];
		}

		//update firmware
		else if (strcmp(argv[i], "-U") == 0 || strcmp(argv[i], "--update-bootloader") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename for the bootloader to update with.\n");
				return(1);
			}
			action = ACTION_UPDATEBOOTLOADER;
			param = argv[++i];
		}

		//self test
		else if (strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "--self-test") == 0) {
			action = ACTION_SELFTEST;
		}
	}

	if (dev.Open() == false) {
		printf("Error opening device.\n");
		return(2);
	}

	printf(" Device: %s, %dMB flash (firmware build %d, flashID %06X)\n", dev.DeviceName, dev.FlashSize / 0x100000, dev.Version, dev.FlashID);
	printf("\n");
	if (dev.Version < required_build && action != ACTION_UPDATEFIRMWARE) {
		char ch;

		printf("Firmware is outdated, the required minimum version is %d\n\n", required_build);
		printf("Press 'y' to upgrade, any other key cancel: \n");
		ch = readKb();
		if (ch == 'Y' || ch == 'y') {
			success = upload_firmware(firmware, firmware_length);
		}
		action = -1;
	}

//	dev.VerifyBootloader();

	switch (action) {

	default:
		break;

	case ACTION_NONE:
		printf("No operation specified.\n");
		success = true;
		break;

	case ACTION_LIST:
		success = fds_list(verbose);
		break;

	case ACTION_UPDATEFIRMWARE:
		success = firmware_update(param);
		break;

	case ACTION_UPDATEBOOTLOADER:
		success = bootloader_update(param);
		break;

	case ACTION_WRITEFLASH:
		if (is_gamedoctor(param)) {
			printf("Detected Game Doctor image.\n");
			success = write_doctor(param);
		}
		else {
			success = write_flash(param, (param2 == 0) ? -1 : atoi(param2));
		}
		break;

	case ACTION_WRITEDOCTORFLASH:
		success = write_doctor(param);
		break;

	case ACTION_READFLASH:
		dev.FlashUtil->ReadHeaders();
		success = read_flash(param, atoi(param2));
		break;

	case ACTION_READFLASHRAW:
		dev.FlashUtil->ReadHeaders();
		success = read_flash_raw(param, atoi(param2));
		break;

	case ACTION_ERASEFLASH:
		dev.FlashUtil->ReadHeaders();
		//erase all slots
		if (params[0] && strcmp(params[0], "all") == 0) {
			if (force == 0) {
				printf("This operation will erase all flash disk slots.\nTo confirm this operation please add --force to the command line.\n\n");
			}
			else {
				printf("Erasing all slots from flash...\n");
				success = dev.Flash->Erase(SLOTSIZE, dev.FlashSize - SLOTSIZE);
			}
		}

		//erase one slot
		else {
			printf("Erase disk image from flash...\n");
			for (i = 0; i < numparams; i++) {
				slot = atoi(params[i]);
				printf("   Slot %d", slot);
				success = erase_slot(slot);
				printf("\n");
//				success = dev.Flash->Erase(slot * SLOTSIZE, SLOTSIZE);
				if (success == false) {
					break;
				}
			}
		}
		break;

	case ACTION_WRITEDISK:
		printf("Writing disk from file...\n");
		success = FDS_writeDisk(param);
		break;

	case ACTION_READDISK:
		printf("Reading disk to file...\n");
		success = FDS_readDisk(param, param2, param3);
		break;

	case ACTION_CHIPERASE:
		printf("Erasing entire flash chip...\n");
		success = chip_erase();
		break;

	case ACTION_SELFTEST:
		printf("Self test...\n");
		success = dev.Selftest();
		break;

	case ACTION_VERIFY:
		printf("Verify integrity of device...\n");
		success = verify_device();
		break;
	}

	dev.Close();

	if (success) {
		printf("Operation completed successfully.\n");
	}
	else {
		printf("Operation failed.\n");
	}

	return(0);
}
