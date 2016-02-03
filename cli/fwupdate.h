#pragma once

bool upload_firmware(uint8_t *firmware, int filesize, int useflash);
bool upload_bootloader(uint8_t *firmware, int filesize);
bool firmware_update(char *filename, int useflash);
bool bootloader_update(char *filename);
