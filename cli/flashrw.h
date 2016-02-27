#pragma once

bool read_flash(char *filename_fds, int slot);
bool read_flash_raw(char *filename, int slot);
bool write_flash(char *filename, int slot);
bool write_doctor(char *file);
bool is_gamedoctor(char *filename);
int find_slot(int slots);
char *get_shortname(char *filename);
bool find_doctors(char *first, char **files, int *numfiles, int maxfiles);

int decompress_lz4(uint8_t *src, uint8_t *dest, int srclen, int destlen);
int compress_lz4(uint8_t *src, int srcsize, uint8_t **dst, int *dstsize);
