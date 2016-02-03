#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "crc32.h"
#include "System.h"

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
	uint32_t oldcrc, crc;

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
