#ifndef __crc32_h__
#define __crc32_h__

#include <stdint.h>

void crc32_gentab();
uint32_t crc32(unsigned char *block,unsigned int length);
uint32_t crc32_byte(uint8_t data,uint32_t crc);
uint32_t crc32_block(uint8_t *data,uint32_t length,uint32_t crc);

#endif
