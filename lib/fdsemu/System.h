#pragma once

uint32_t getTicks();
void utf8_to_utf16(uint16_t *dst, char *src, size_t dstSize);
char readKb();
void sleep_ms(int millisecs);
