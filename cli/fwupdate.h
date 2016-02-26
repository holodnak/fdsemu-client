#pragma once

int detect_firmware_build(uint8_t *fw, int len);
int detect_bootloader_version(uint8_t *fw, int len);
uint32_t bootloader_get_crc32(uint8_t *fw, int len);
bool upload_firmware(uint8_t *firmware, int filesize, int useflash);
bool upload_bootloader(uint8_t *firmware, int filesize);
bool firmware_update(char *filename, int useflash);
bool bootloader_update(char *filename);
