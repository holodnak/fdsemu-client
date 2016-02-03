#pragma once

#include "Device.h"

extern CDevice dev;

bool loadfile(char *filename, uint8_t **buf, int *filesize);
bool file_exists(char *fn);
