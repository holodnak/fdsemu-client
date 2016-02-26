#pragma once

char *messages_get();
void messages_printf(char *str, ...);

void raw_to_raw03(uint8_t *raw, int rawSize);
int fds_to_bin(uint8_t *dst, uint8_t *src, int dstSize);
int gameDoctor_to_bin(uint8_t *dst, uint8_t *src, int dstSize);
bool raw03_to_fds(uint8_t *raw, uint8_t *fds, int rawsize);
int raw03_to_gd(uint8_t *raw, uint8_t *fds, int rawsize);
uint32_t chksum_calc(uint8_t *buf, int size);
void bin_to_raw03(uint8_t *bin, uint8_t *raw, int binSize, int rawSize);
void raw03_to_bin(uint8_t *raw, int rawSize, uint8_t **_bin, int *_binSize, int *dataSize);
void raw_to_bin(uint8_t *raw, int rawSize, uint8_t **_bin, int *_binSize, int *dataSize);
