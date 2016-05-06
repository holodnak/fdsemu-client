#pragma once

#include <stdint.h>
#include "Device.h"
#include "Disk.h"

//maximum number of slots possibly used by one game
#define MAX_SLOTS		32

#define MIN(aa,bb)	(((aa) > (bb)) ? (bb) : (aa))

bool loadfile(char *filename, uint8_t **buf, int *filesize);

class CFdsemu {
private:
	char error[1024];
	bool success;

protected:
	void SetError(const char *str);
	void ClearError();

	bool FindSlots(CDisk *disk, int *slots, int maxslots);

public:
	CDevice *dev;

public:
	CFdsemu(CDevice *d);
	~CFdsemu();

	bool Init();
	bool CheckDevice();
	bool GetError(char *str, int len);
	bool ReadDisk(uint8_t **raw, int *rawsize);
	bool WriteDisk(uint8_t *bin, int binsize);
	int ParseDiskData(uint8_t *raw, int rawsize, char **output);
	bool WriteFlashFastFDS(char *filename, TCallback cb, void *user);
	bool WriteFlashFDS(char *filename, TCallback cb, void *user);
	bool WriteFlashGD(char *filename, TCallback cb, void *user);
	bool WriteFlashFastGD(char *filename, TCallback cb, void *user);
	bool WriteFlash(char *filename, TCallback cb, void *user);
	bool WriteFlashRaw(int addr, uint8_t *buf, int len);
	bool ReadFlash(int slot, uint8_t **buf, int *bufsize);
	bool ReadFlashRaw(int addr, uint8_t **buf, int len);
	bool Erase(int slot);
	bool EraseSlot(int slot);
};
