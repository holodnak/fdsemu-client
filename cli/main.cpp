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
#include "diskutil.h"
#include "diskrw.h"
#include "flashrw.h"

#if defined WIN32
char host[] = "Win32";
#elif defined WIN64
char host[] = "Win64";
#elif defined __linux__
char host[] = "Linux";
#elif defined(__APPLE__)
char host[] = "OSX"
#else
char host[] = "Unknown";
#endif

CDevice dev;

enum {
	ACTION_INVALID = -1,

	ACTION_NONE = 0,
	ACTION_HELP,
	ACTION_CONVERT,

	ACTION_LIST,
	ACTION_READFLASH,
	ACTION_READFLASHRAW,
	ACTION_WRITEFLASH,
	ACTION_WRITEDOCTORFLASH,
	ACTION_ERASEFLASH,

	ACTION_READDISK,
	ACTION_READDISKRAW,
	ACTION_READDISKBIN,
	ACTION_READDISKDOCTOR,
	ACTION_WRITEDISK,

	ACTION_UPDATEFIRMWARE,
	ACTION_UPDATEFIRMWARE2,
	ACTION_UPDATEBOOTLOADER,
	ACTION_UPDATELOADER,

	ACTION_CHIPERASE,
	ACTION_SELFTEST,
	ACTION_VERIFY,
};


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

#if defined(WIN32)
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

	printf("\n  usage: %s <options> <command> <file(s)>\n\n%s", "fdsemu-cli", usagestr);
}

bool upload_firmware(uint8_t *firmware, int filesize, int useflash)
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
	if ((dev.Version > 792) && (useflash == 0)) {
		printf("uploading new firmware to sram\n");
		if (!dev.Sram->Write(buf, 0x0000, 0x8000)) {
			printf("Write failed.\n");
			return false;
		}
	}

	//older firmware store the firmware image into flash memory
	else {
		printf("uploading new firmware to flash");
		if (!dev.Flash->Write(buf, 0x8000, 0x8000, 0)) {
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
	
	printf("Updated bootloader, old crc = %08X, new crc = %08X\n", oldcrc, crc);
	return(true);
}

bool firmware_update(char *filename, int useflash)
{
	uint8_t *firmware;
	int filesize;
	bool ret = false;

	//try to load the firmware image
	if (loadfile(filename, &firmware, &filesize) == false) {
		printf("Error loading firmware file %s'\n", filename);
		return(false);
	}

	ret = upload_firmware(firmware, filesize, useflash);

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

uint8_t *find_string(uint8_t *str, uint8_t *buf, int len)
{
	int identlen = strlen((char*)str);
	uint8_t byte, *ptr = buf;
	int i;
	uint8_t *ret = 0;

	for (i = 0; i < (len - identlen); i++, buf++) {

		//first byte is a match, continue checking
		if (*buf == (uint8_t)str[0]) {

			//check for match
			if (memcmp(buf, str, identlen) == 0) {
				ret = buf;
				break;
			}
		}
	}
	return(ret);
}

uint8_t ident_bootloader[] = "*BOOT2*";

bool bootloader_is_valid(uint8_t *fw, int len)
{
	uint8_t *buf;

	if (len > 4096) {
		return(false);
	}
	buf = find_string(ident_bootloader, fw, len);
	return(buf == 0 ? false : true);
}

uint32_t bootloader_get_crc32(uint8_t *fw, int len)
{
	uint8_t *buf;
	uint32_t *buf32;
	uint32_t crc;

	if (len > 4096) {
		printf("bootloader image is too large.\n");
		return(0);
	}
	buf = new uint8_t[4096 + 8];
	buf32 = (uint32_t*)buf;
	memset(buf, 0, 4096 + 8);
	memcpy(buf, fw, len);

	//insert id
	buf32[(0x1000) / 4] = 0xCAFEBABE;

	//calculate the crc32 checksum
	crc = crc32(buf, 0x1000 + 4);

	delete[] buf;

	return(crc);
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
	uint8_t *ptr = fw;
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

int detect_bootloader_version(uint8_t *fw, int len)
{
	const char ident[] = "*BOOT2*";
	int identlen = strlen(ident);
	uint8_t *ptr = fw;
	int i;
	int ret = -1;

	for (i = 0; i < (len - identlen); i++, fw++) {

		//first byte is a match, continue checking
		if (*fw == (uint8_t)ident[0]) {

			//check for match
			if (memcmp(fw, ident, identlen) == 0) {
				ret = *(fw + identlen);
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
extern unsigned char bootloader[];
extern int firmware_length;
extern int bootloader_length;

typedef struct arg_s {
	int action;
	char cshort;
	char clong[32];
	int numparams;
	int *flagptr;
	char description[256];
} arg_t;

#define ARG_ACTION(ac,sh,lo,np,de)		{ac,sh,lo,np,0,de}
#define ARG_FLAG(va,sh,lo,de)			{-1,sh,lo,0,va,de}
#define ARG_END()						{0,0,"",0,0,""}

arg_t args[] = {
	ARG_ACTION	(ACTION_HELP,				'h',	"--help",					0,	"Display this message"),
	ARG_ACTION	(ACTION_LIST,				'l',	"--list",					0,	"List all disks stored in flash"),
	ARG_ACTION	(ACTION_SELFTEST,			0,		"--self-test",				0,	"Perform FDSemu self-test"),
	ARG_ACTION	(ACTION_CHIPERASE,			0,		"--chip-erase",				0,	"Erase entire flash chip"),
	ARG_ACTION	(ACTION_UPDATEFIRMWARE,		0,		"--update-firmware",		1,	"Update firmware from file (using sram)"),
	ARG_ACTION	(ACTION_UPDATEFIRMWARE2,	0,		"--update-firmware-flash",	1,	"Update firmware from file (using flash)"),
	ARG_ACTION	(ACTION_UPDATEBOOTLOADER,	0,		"--update-bootloader",		1,	"Update bootloader from file"),

	ARG_ACTION	(ACTION_WRITEDISK,			'w',	"--write-disk",				-1,	"Write disk from disk image"),
	ARG_ACTION	(ACTION_READDISK,			'r',	"--read-disk",				1,	"Read disk to fwNES format"),
	ARG_ACTION	(ACTION_READDISKBIN,		0,		"--read-disk-bin",			1,	"Read disk to bin format"),
	ARG_ACTION	(ACTION_READDISKRAW,		0,		"--read-disk-raw",			1,	"Read disk to raw format"),
	ARG_ACTION	(ACTION_READDISKDOCTOR,		0,		"--read-disk-doctor",		1,	"Read disk to game doctor format"),

	ARG_ACTION	(ACTION_ERASEFLASH,			'e',	"--erase",					-1,	"Erase list of slots"),
	ARG_ACTION	(ACTION_WRITEFLASH,			'f',	"--write-flash",			-1,	"Write list of disks to flash"),

	ARG_FLAG	(&force,	0,	"--force",		"Force an operation (depreciated)"),
	ARG_FLAG	(&verbose,	0,	"--verbose",	"More verbose output"),

	ARG_END()
};

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
	int required_version = -1;
	uint32_t required_crc32 = 0;

	crc32_gentab();

	printf("fdsemu-cli v%d.%d.%d (%s) by James Holodnak, based on code by loopy\n", VERSION / 100, VERSION % 100, BUILDNUM, host);

	required_build = detect_firmware_build((uint8_t*)firmware, firmware_length);
	required_version = detect_bootloader_version((uint8_t*)bootloader, bootloader_length);
	required_crc32 = bootloader_get_crc32((uint8_t*)bootloader, bootloader_length);

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
		else if (strcmp(argv[i], "--update-firmware-flash") == 0) {
			if ((i + 1) >= argc) {
				printf("\nPlease specify a filename for the firmware to update with.\n");
				return(1);
			}
			action = ACTION_UPDATEFIRMWARE2;
			param = argv[++i];
		}

		//update firmware
		else if (strcmp(argv[i], "--update-bootloader") == 0) {
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
	if (dev.Version < required_build && action != ACTION_UPDATEFIRMWARE && action != ACTION_UPDATEFIRMWARE2) {
		char ch;

		printf("Firmware is outdated, the required minimum version is %d\n\n", required_build);
		printf("Press 'y' to upgrade, any other key cancel: \n");
		ch = readKb();
		if (ch == 'Y' || ch == 'y') {
			success = upload_firmware(firmware, firmware_length, 0);
		}
		action = ACTION_INVALID;
	}

	if (dev.VerifyBootloader() != required_crc32 && action != ACTION_UPDATEBOOTLOADER && action != ACTION_UPDATEFIRMWARE && action != ACTION_UPDATEFIRMWARE2) {
		char ch;

		printf("Bootloader is outdated.\n\n");
		printf("Press 'y' to upgrade, any other key cancel: \n");
		ch = readKb();
		if (ch == 'Y' || ch == 'y') {
			success = upload_bootloader(bootloader, bootloader_length);
		}
		action = ACTION_INVALID;
	}

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
		success = firmware_update(param, 0);
		break;

	case ACTION_UPDATEFIRMWARE2:
		printf("Updating firmware by flash\n");
		success = firmware_update(param, 1);
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
		success = writeDisk(param);
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
