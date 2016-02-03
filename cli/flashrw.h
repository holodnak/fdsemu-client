#pragma once

bool read_flash(char *filename_fds, int slot);
bool read_flash_raw(char *filename, int slot);
bool write_flash(char *filename, int slot);
bool write_doctor(char *file);
