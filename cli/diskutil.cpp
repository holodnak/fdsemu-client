#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Device.h"
#include "diskutil.h"

#include <stdio.h>
#include <stdarg.h>

typedef struct message_s {
	char *str;
	struct message_s *next;
} message_t;

static message_t *messages = 0;

void messages_add(char *str)
{
	message_t *m, *msg = new message_t;

	msg->str = strdup(str);
	msg->next = 0;
	if (messages == 0) {
		messages = msg;
	}
	else {
		m = messages;
		while (m->next) {
			m = m->next;
		}
		m->next = msg;
	}
}

void messages_clear()
{
	message_t *pm, *m = messages;

	while (m) {
		pm = m;
		m = m->next;
		free(pm->str);
		delete pm;
	}
	messages = 0;
}

char *messages_get()
{
	message_t *m = messages;
	int len = 0;
	char *ret;

	while (m) {
		len += strlen(m->str);
		m = m->next;
	}
	m = messages;
	ret = (char*)malloc(len + 1);
	memset(ret, 0, len + 1);
	while (m) {
		strcat(ret, m->str);
		m = m->next;
	}
	return(ret);
}

void messages_printf(char str[], ...)
{
	char buffer[256];

	va_list args;
	va_start(args, str);
	vsprintf(buffer, str, args);
	messages_add(buffer);
	va_end(args);
}

uint8_t raw_to_raw03_byte_48mhz(uint8_t raw)
{
	if (raw < 0x28)
		return(3);
	else if (raw < 0x4D)
		return(0);
	else if (raw < 0x6D)
		return(1);
	else if (raw < 0x8D)
		return(2);
	return(3);
}

uint8_t raw_to_raw03_byte_72mhz(uint8_t raw)
{
	if (raw < 0x40)
		return(3);
	else if (raw < 0x80)
		return(0);
	else if (raw < 0xA8)
		return(1);
	else if (raw < 0xE0)
		return(2);
	return(3);
}

//wow what a kludge :(
uint8_t raw_to_raw03_byte(uint8_t raw)
{
	return(raw_to_raw03_byte_72mhz(raw));
}

//Turn raw data from adapter to pulse widths (0..3)
//Input capture clock is 6MHz.  At 96.4kHz (FDS bitrate), 1 bit ~= 62 clocks
//We could be clever about this and account for drive speed fluctuations, etc. but this will do for now
void raw_to_raw03(uint8_t *raw, int rawSize) {
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
//	messages_printf("copying block type %d, size = %d, crc = %04X\r\n", dst[1],size + 2, crc);
}

//Adds GAP + GAP end (0x80) + CRCs to .FDS image
//Returns size (0=error)
int fds_to_bin(uint8_t *dst, uint8_t *src, int dstSize) {
	int i = 0, o = 0;

	//check *NINTENDO-HVC* header
	if (src[0] != 0x01 || src[1] != 0x2a || src[2] != 0x4e) {
		messages_printf("Not an FDS file.\r\n");
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
			messages_printf("Out of space (%d bytes short), adjust GAP size?\r\n", (o + 16 + 3 + GAP + size + 3) - dstSize);
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
		messages_printf("Not GD format.\r\n");
		return 0;
	}
	memset(dst, 0, dstSize);

	//	messages_printf("converting image, max size = %d\r\n", dstSize);

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
		//		messages_printf("copying blocks 3+4, size3 = %d, size4 = %d (i = %d, o = %d)\r\n", 16 + 3, size + 3, i, o);
		if (o + 16 + 3 + GAP + size + 3 > dstSize) {    //end + block3 + crc + gap + end + block4 + crc
			messages_printf("Out of space (%d bytes short), adjust GAP size?\r\n", (o + 16 + 3 + GAP + size + 3) - dstSize);
			return 0;
		}
		copy_block(dst + o, src + i, 16);
		i += 16 + 2;
		o += 16 + 3 + GAP;

		copy_block(dst + o, src + i, size);
		i += size + 2;
		o += size + 3 + GAP;
	}
	return o;
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
	i = 0;
	i = 0x700;
	for (len = 0; i<0x2000 * 8; i++) {
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
		messages_printf("Out of space\r\n");
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
							   //messages_printf("Disk end\r\n"); 
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
				 //messages_printf("glitch(%d) @ %X(%X.%d)\r\n", src[in], in, out/8, out%8);
			out++;
			bitval = 0;
			break;
		}
		in++;
	} while (out<outEnd);
	if (dst[*outP] != blockType) {
		messages_printf("Wrong block type %X(%X)-%X(%X) (found %d, expected %d)\r\n", start, *outP, in, out - 1, dst[*outP], blockType);
		return false;
	}
	out = out / 8 - 2;

	//messages_printf("Out%d %X(%X)-%X(%X)\r\n", blockType, start, *outP, in, out-1);

	if (calc_crc(dst + *outP, blockSize + 2)) {
		uint16_t crc1 = (dst[out + 1] << 8) | dst[out];
		dst[out] = 0;
		dst[out + 1] = 0;
		uint16_t crc2 = calc_crc(dst + *outP, blockSize + 2);
		messages_printf("Bad CRC (%04X!=%04X)\r\n", crc1, crc2);
	}

	dst[out] = 0;     //clear CRC
	dst[out + 1] = 0;
	dst[out + 2] = 0;   //+spare bit
	*inP = in;
	*outP = out;
	return true;
}

//Simplified disk decoding.  This assumes disk will follow standard FDS file structure
bool raw03_to_fds(uint8_t *raw, uint8_t *fds, int rawsize) {
	int in, out;
	int dstsize = FDSSIZE;

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

int raw03_to_gd(uint8_t *raw, uint8_t *fds, int rawsize) {
	int in, out;
	int dstsize = FDSSIZE + 0x10000;

	memset(fds, 0, dstsize);

	//lead-in can vary a lot depending on drive, scan for first block to get our bearings
	in = findFirstBlock(raw) - MIN_GAP_SIZE;
	if (in<0)
		return -1;

	out = 3;

	if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 0x38, 1))
		return -1;

	//insert zeros for the false crc bytes in gd image
	out += 2;

	if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 2, 2))
		return -1;

	//fixup the third byte
	fds[2] = 0x80 | fds[out - 1];

	//insert zeros for the false crc bytes in gd image
	out += 2;

	do {
		if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 16, 3))
			break;

		//insert zeros for the false crc bytes in gd image
		out += 2;

		if (!block_decode(fds, raw, &in, &out, rawsize, dstsize + 2, 1 + (fds[out - 18 + 13] | (fds[out - 18 + 14] << 8)), 4))
			break;

		//insert zeros for the false crc bytes in gd image
		out += 2;

	} while (in<rawsize);
	return out;
}

//make raw0-3 from flash image (sans header)
void bin_to_raw03(uint8_t *bin, uint8_t *raw, int binSize, int rawSize) {
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
	messages_printf("mark gap %X-%X\r\n", i + 1, gapEnd);
}

//For information only for now.  This checks for standard file format
static int verify_block(uint8_t *bin, int start, int *reverse) {
	enum { MAX_GAP = (976 + 200) / 8, MIN_GAP = (976 - 200) / 8 };
	static const uint8_t next[] = { 0,2,3,4,3 };
	static int last = 0;
	static int lastLen = 0;
	static int blockCount = 0;
	static int totalLen = 0;

	//kludge to reset the variables
	if (bin == 0) {
		last = 0;
		lastLen = 0;
		blockCount = 0;
		totalLen = 0;
		return(0);
	}
	int len = 0;
	uint8_t type = bin[start];

	messages_printf("%d:%X", ++blockCount, type);

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
		messages_printf(" bad block (%X)\r\n", start);
		return(0);
	}
	messages_printf(" %X-%X / %X-%X(%X)", reverse[start], reverse[start + len], start, start + len, len);

	if ((!last && type != 1) || (last && type != next[bin[last]]))
		messages_printf(", wrong filetype");
	if (calc_crc(bin + start, len + 2) != 0)
		messages_printf(", bad CRC");
	if (last && (last + lastLen + MAX_GAP)<start)
		messages_printf(", lost block?");
	if (last + lastLen + MIN_GAP>start)
		messages_printf(", block overlap?");
	//if(type==3 && ...)    //check other fields in file header?

	messages_printf("\r\n");
	last = start;
	lastLen = len;
	totalLen += len;
	return(totalLen);
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

dataSize is total size of all data on the disk (excluding gaps and gap end marker)
*/
void raw03_to_bin(uint8_t *raw, int rawSize, uint8_t **_bin, int *_binSize, int *dataSize) {
	enum {
		BINSIZE = 0xa0000,
		POST_GLITCH_GARBAGE = 16,
		LONG_POST_GLITCH_GARBAGE = 64,
		LONG_GAP = 100*8,   //976 typ.
		SHORT_GAP = 16,
	};
	int in, out;
	uint8_t *bin;
	int *reverse;
	int glitch;
	int zeros;

	messages_clear();
	verify_block(0, 0, 0);
	bin = (uint8_t*)malloc(BINSIZE);
	reverse = (int*)malloc(BINSIZE*sizeof(int));
	memset(bin, 0, BINSIZE);

	//--- assume any glitch is OOB, mark a run of zeros near a glitch as a gap start.

	int junk = 0;
	glitch = 0;
	zeros = 0;
	junk = 0;
	in = 0;
//	in = 4096;
	for (; in<rawSize; in++) {
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
		messages_printf("header at %X\r\n", in);
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
			messages_printf("crc found %X-%X\r\n", in, out);
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
			//messages_printf("Encoding error @ %X(%X)\r\n",in,out/8);
		default: //anything else (glitch)
			out++;
			bitval = 0;
			break;
		}
		reverse[out / 8] = in;
	}
	//last block
	*dataSize = verify_block(bin, lastBlockStart, reverse);

	*_bin = bin;
	*_binSize = out / 8 + 1;
	free(reverse);
}

void raw_to_bin(uint8_t *raw, int rawSize, uint8_t **_bin, int *_binSize, int *dataSize) {
	uint8_t *tmpraw;
	int i;

	tmpraw = (uint8_t*)malloc(rawSize);
	for (i = 0; i < rawSize; i++) {
		tmpraw[i] = raw_to_raw03_byte(raw[i]);
	}
	raw03_to_bin(tmpraw, rawSize, _bin, _binSize, dataSize);
	free(tmpraw);
}
