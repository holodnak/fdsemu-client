#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "Fdsemu.h"
#include "flashrw.h"
#include "Disk.h"

//allocate buffer and read whole file
bool loadfile(char *filename, uint8_t **buf, int *filesize)
{
	FILE *fp;
	int size;
	bool result = false;

	//check if the pointers are ok
	if (filename == 0 || buf == 0 || filesize == 0) {
		return(false);
	}

	//open file
	if ((fp = fopen(filename, "rb")) == 0) {
		return(false);
	}

	//get file size
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//allocate buffer
	*buf = (uint8_t*)malloc(size);

	//read in file
	*filesize = fread(*buf, 1, size, fp);

	//close file and return
	fclose(fp);
	return(true);
}

void savefile(char *filename, uint8_t *buf, int buflen)
{
	FILE *fp;

	if ((fp = fopen(filename, "wb")) == 0) {
		return;
	}

	fwrite(buf, buflen, 1, fp);
	fclose(fp);
}

void CFdsemu::SetError(const char *str)
{
	strcat(error, str);
	success = false;
}

void CFdsemu::ClearError()
{
	memset(error, 0, 1024);
	success = true;
}

bool CFdsemu::FindSlots(CDisk *disk, int *slots, int maxslots)
{
	TFlashHeader *headers;
	int i, n, sides;

	//reset all slots
	for (i = 0; i < maxslots; i++) {
		slots[i] = 0xFFFF;
	}

	//get copy of flash headers
	headers = dev->FlashUtil->GetHeaders();

	//save number of sides this disk image uses
	sides = disk->GetSides();

	for (n = 0, i = 0; i < (int)(dev->Slots); i++) {

		//check if slot is empty
		if (headers[i].filename[0] == 0xFF) {

			//save empty slot in the slot list
			slots[n++] = i;
		}

		//if we have too many slots, quit
		if (n > maxslots) {
			return(false);
		}

		//if we have found enough empty slots, stop looking and return success
		if (n == sides) {
			return(true);
		}
	}

	//if we get here we didnt find enough empty slots
	return(false);
}

CFdsemu::CFdsemu(CDevice *d)
{
	dev = d;
	ClearError();
}

CFdsemu::~CFdsemu()
{
	dev->Close();
}

bool CFdsemu::Init()
{
	bool ret = dev->Open();

	if (ret == true) {
		dev->FlashUtil->ReadHeaders();
		ClearError();
	}
	else {
		SetError("Error opening device.  Is the FDSemu plugged in?\n");
	}
	return(ret);
}

bool CFdsemu::CheckDevice()
{
	bool ret;

	ret = dev->Reopen();
	if (ret == false) {
		SetError("FDSemu not detected, please re-insert the device.\n");
	}
	return(ret);
}

bool CFdsemu::GetError(char *str, int len)
{
	strncpy(str, error, len);
	return(success);
}

bool CFdsemu::ReadDisk(uint8_t **raw, int *rawsize)
{
	enum {
		READBUFSIZE = 0x90000,
		LEADIN = 26000
	};

	//	FILE *f;
	uint8_t *readBuf = NULL;
	int result;
	int bytesIn = 0;

	*raw = 0;
	*rawsize = 0;
	if (CheckDevice() == false) {
		return(false);
	}
	ClearError();

	//if(!(dev_readIO()&MEDIA_SET)) {
	//    printf("Warning - Disk not inserted?\n");
	//}
	if (!dev->DiskReadStart()) {
		SetError("diskreadstart failed\n");
		return false;
	}

	readBuf = (uint8_t*)malloc(READBUFSIZE);
	do {
		result = dev->DiskRead(readBuf + bytesIn);
		bytesIn += result;
		//		if (!(bytesIn % ((DISK_READMAX)* 32)))
		//			printf(".");
	} while (result == DISK_READMAX && bytesIn<READBUFSIZE - DISK_READMAX);

	if (result<0 || (bytesIn - (LEADIN / 8)) <= 0) {
		SetError("read error\n");
		free(readBuf);
		return false;
	}

	//eat up the lead-in
	*raw = (uint8_t*)malloc(bytesIn - (LEADIN / 8));
	*rawsize = bytesIn - (LEADIN / 8);
	memcpy(*raw, readBuf + (LEADIN / 8), *rawsize);

	free(readBuf);

	return true;
}

bool CFdsemu::WriteDisk(uint8_t *bin, int binsize)
{
	const int ZEROSIZE = 0x10000 + 8192;
	uint8_t *zero = 0;
	bool ret = false;

	if (CheckDevice() == false) {
		return(false);
	}
	ClearError();

	zero = (uint8_t*)malloc(ZEROSIZE);
	memset(zero, 0, ZEROSIZE);

	do {
		//zero out the sram
		if (dev->Sram->Write(zero, 0, ZEROSIZE) == false) {
//			printf("Sram write failed (zero).\n");
			break;
		}

		//write disk bin image to sram
		if (dev->Sram->Write(bin, 0, binsize) == false) {
//			printf("Sram write failed (bin).\n");
			break;
		}

		//tell fdsemu to start writing the disk
		if (!dev->DiskWriteStart()) {
//			printf("DiskWriteStart failed (bin).\n");
			break;
		}
		ret = true;

	} while (0);

	free(zero);
	return(ret);
}

#include "diskutil.h"

int CFdsemu::ParseDiskData(uint8_t *raw, int rawsize, char **output)
{
	uint8_t *binBuf;
	int binSize, datasize;
	bool ret = false;

	raw_to_bin(raw, rawsize, &binBuf, &binSize, &datasize);
	free(binBuf);
	messages_printf("Total size of disk data: %d bytes\r\n", datasize);
	*output = messages_get();
	return(datasize);
}

#define HEADER_SIZE		240
#define HEADER_OWNERID	242
#define HEADER_NEXTID	244
#define HEADER_SAVEID	246
#define HEADER_FLAGS		248

//write disk image to flash
bool CFdsemu::WriteFlashFDS(char *filename, TCallback cb, void *user)
{
	TFlashHeader *headers;
	uint8_t *slotdata, *verifybuf, *verifybuf2, *bin, *pbin;
	int binsize, i, slots[MAX_SLOTS + 1];
	CDisk disk;
	char *shortname;
	bool ret = true;

	//semi-kludge to ensure the last disk side points to nothing
	slots[MAX_SLOTS] = 0xFFFF;

	//load the disk image
	disk.LoadFDS(filename);

	//find enough slots to store the disk image
	if (FindSlots(&disk, slots, MAX_SLOTS) == false) {
		MessageBox(0, "Error finding adequate slots for storage disk image.", "Error", MB_OK);
		return(false);
	}

	//clear the header and copy the disk image filename
	shortname = get_shortname(filename);

	//allocate memory for slot data
	slotdata = (uint8_t*)malloc(0x10000);
	verifybuf = (uint8_t*)malloc(0x10000);
	verifybuf2 = (uint8_t*)malloc(0x10000);

	cb(user, 0, disk.GetSides());

	//write each side to flash
	for (i = 0; i < disk.GetSides(); i++) {

		//copy bin to the rest of the slotdata buffer
		if (disk.GetBin(i, &bin, &binsize) == false) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: error getting bin for disk", "Error", MB_OK);
			ret = false;
			break;
		}

		//eat up the lead-in, fdsemu will provide that
		while (*bin == 0) {
			bin++;
			binsize--;
		}

		if (binsize >= (0x10000 - 256)) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: bin for disk side is too big...try game doctor format?", "Error", MB_OK);
			ret = false;
			break;
		}

		//clear slotdata buffer
		memset(slotdata, 0, 0x10000);

		//setup the header
		//copy the filename if this is the first block
		if (i == 0) {
			strncpy((char*)slotdata, shortname, 240);
		}

		//this bit tells fdsemu that the "nextid" byte is valid
		slotdata[HEADER_FLAGS] = 0x20;

		//fixup pointer to owner id (first slot of a series of slots)
		slotdata[HEADER_OWNERID + 0] = (uint8_t)(slots[0] >> 0);
		slotdata[HEADER_OWNERID + 1] = (uint8_t)(slots[0] >> 8);

		//fixup pointer to next side id
		slotdata[HEADER_NEXTID + 0] = (uint8_t)(slots[i + 1] >> 0);
		slotdata[HEADER_NEXTID + 1] = (uint8_t)(slots[i + 1] >> 8);

		//fixup bin size
		slotdata[HEADER_SIZE + 0] = (uint8_t)(binsize >> 0);
		slotdata[HEADER_SIZE + 1] = (uint8_t)(binsize >> 8);

		//copy the bin data
		memcpy(slotdata + 256, bin, binsize);

		//write the data to flash
		cb(user, 1, i);

		if (dev->Flash->Write(slotdata, slots[i] * SLOTSIZE, SLOTSIZE, cb, user) == false) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: error writing to flash", "Error", MB_OK);
			break;
		}

		if (dev->Flash->Read(verifybuf, slots[i] * SLOTSIZE, SLOTSIZE) == false) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: error reading back disk side for verification", "Error", MB_OK);
			ret = false;
			break;
		}

		//if it doesnt match
		if (memcmp(verifybuf, slotdata, SLOTSIZE) != 0) {

			//re-read
			if (dev->Flash->Read(verifybuf2, slots[i] * SLOTSIZE, SLOTSIZE) == false) {
				MessageBox(0, "CFdsemu::WriteFlashFDS: error reading back disk side for verification (second pass)", "Error", MB_OK);
				ret = false;
				break;
			}

			//compare the two verify's
			if (memcmp(verifybuf, verifybuf2, SLOTSIZE) != 0) {
				char tmpstr[256];

				sprintf(tmpstr, "CFdsemu::WriteFlashFDS: verification failed for slot %d", slots[i]);
				savefile("verify.buffer1.dump", verifybuf, 0x10000);
				savefile("verify.buffer2.dump", verifybuf2, 0x10000);
				savefile("verify.slotdata.dump", slotdata, 0x10000);
				MessageBox(0, tmpstr, "Error", MB_OK);
				ret = false;
				break;
			}

			else {
				MessageBox(0, "CFdsemu::WriteFlashFDS:  Read back two different verification data from flash...continuing, data probably ok.", "Warning", MB_OK);
			}
		}
	}

	//free slot buffer
	free(slotdata);
	free(verifybuf);
	free(verifybuf2);

	return(ret);
}

//write disk image to flash
bool CFdsemu::WriteFlashFastFDS(char *filename, TCallback cb, void *user)
{
	TFlashHeader *headers;
	uint8_t *slotdata, *verifybuf, *verifybuf2, *bin, *pbin;
	int binsize, i, slots[MAX_SLOTS + 1];
	CDisk disk;
	char *shortname;
	bool ret = true;

	//semi-kludge to ensure the last disk side points to nothing
	slots[MAX_SLOTS] = 0xFFFF;

	//load the disk image
	disk.LoadFDS(filename);

	//find enough slots to store the disk image
	if (FindSlots(&disk, slots, MAX_SLOTS) == false) {
		MessageBox(0, "Error finding adequate slots for storage disk image.", "Error", MB_OK);
		return(false);
	}

	//clear the header and copy the disk image filename
	shortname = get_shortname(filename);

	//allocate memory for slot data
	slotdata = (uint8_t*)malloc(0x10000);
	verifybuf = (uint8_t*)malloc(0x10000);
	verifybuf2 = (uint8_t*)malloc(0x10000);

	cb(user, 0, disk.GetSides());

	//write each side to flash
	for (i = 0; i < disk.GetSides(); i++) {

		//copy bin to the rest of the slotdata buffer
		if (disk.GetBin(i, &bin, &binsize) == false) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: error getting bin for disk", "Error", MB_OK);
			ret = false;
			break;
		}

		//eat up the lead-in, fdsemu will provide that
		while (*bin == 0) {
			bin++;
			binsize--;
		}

		if (binsize >= (0x10000 - 256)) {
			MessageBox(0, "CFdsemu::WriteFlashFDS: bin for disk side is too big...try game doctor format?", "Error", MB_OK);
			ret = false;
			break;
		}

		//clear slotdata buffer
		memset(slotdata, 0, 0x10000);

		//setup the header
		//copy the filename if this is the first block
		if (i == 0) {
			strncpy((char*)slotdata, shortname, 240);
		}

		//this bit tells fdsemu that the "nextid" byte is valid
		slotdata[HEADER_FLAGS] = 0x20;

		//fixup pointer to owner id (first slot of a series of slots)
		slotdata[HEADER_OWNERID + 0] = (uint8_t)(slots[0] >> 0);
		slotdata[HEADER_OWNERID + 1] = (uint8_t)(slots[0] >> 8);

		//fixup pointer to next side id
		slotdata[HEADER_NEXTID + 0] = (uint8_t)(slots[i + 1] >> 0);
		slotdata[HEADER_NEXTID + 1] = (uint8_t)(slots[i + 1] >> 8);

		//fixup bin size
		slotdata[HEADER_SIZE + 0] = (uint8_t)(binsize >> 0);
		slotdata[HEADER_SIZE + 1] = (uint8_t)(binsize >> 8);

		//copy the bin data
		memcpy(slotdata + 256, bin, binsize);

		cb(user, 1, i);

		//write the data to sram
		if (dev->Sram->Write(slotdata, 0, SLOTSIZE) == false) {
			if (dev->Sram->Write(slotdata, 0, SLOTSIZE) == false) {
				MessageBox(0, "CFdsemu::WriteFlashFastFDS: error writing to sram", "Error", MB_OK);
				ret = false;
				break;
			}
		}

		dev->Sram->Transfer(slots[i]);
	}

	//free slot buffer
	free(slotdata);
	free(verifybuf);
	free(verifybuf2);

	return(ret);
}

bool CFdsemu::WriteFlashGD(char *filename, TCallback cb, void *user)
{
	TFlashHeader *headers, *header;
	uint8_t *slotdata, *verifybuf, *bin, *cbin;
	int binsize, cbinsize, i, slots[MAX_SLOTS + 1];
	CDisk disk;
	char *shortname;
	bool ret = true;

	//semi-kludge to ensure the last disk side points to nothing
	slots[MAX_SLOTS] = 0xFFFF;

	//load the disk image
	disk.LoadGD(filename);

	//find enough slots to store the disk image
	if (FindSlots(&disk, slots, MAX_SLOTS) == false) {
		MessageBox(0, "Error finding adequate slots for storage disk image.", "Error", MB_OK);
		return(false);
	}

	//clear the header and copy the disk image filename
	shortname = get_shortname(filename);

	//allocate memory for slot data
	slotdata = (uint8_t*)malloc(0x10000);
	verifybuf = (uint8_t*)malloc(0x10000);

	cb(user, 0, disk.GetSides());

	//write each side to flash
	for (i = 0; i < disk.GetSides(); i++) {

		//copy bin to the rest of the slotdata buffer
		if (disk.GetBin(i, &bin, &binsize) == false) {
			MessageBox(0, "CFdsemu::WriteFlashGD: error getting bin for disk", "Error", MB_OK);
			ret = false;
			break;
		}

		//compress the bin data
		if (compress_lz4(bin, binsize, &cbin, &cbinsize) != 0) {
			MessageBox(0, "CFdsemu::WriteFlashGD: error compressing disk image", "Error", MB_OK);
			ret = false;
			break;
		}

		if (cbinsize >= (0x10000 - 256)) {
			MessageBox(0, "CFdsemu::WriteFlashGD: bin for disk side is too big after compression, cannot use disk image", "Error", MB_OK);
			delete[] cbin;
			ret = false;
			break;
		}

		//clear slotdata buffer
		memset(slotdata, 0, 0x10000);

		//setup the header

		//this bit tells fdsemu that the "nextid" byte is valid
		slotdata[HEADER_FLAGS] = 0x20;

		//this bit tells fdsemu that image is compressed and read-only, game doctor image
		slotdata[HEADER_FLAGS] |= 0xC0 | 0x01;

		//fixup owner id (first slot of a series of slots)
		slotdata[HEADER_OWNERID + 0] = (uint8_t)(slots[0] >> 0);
		slotdata[HEADER_OWNERID + 1] = (uint8_t)(slots[0] >> 8);

		//fixup next side id
		if (disk.IsSaveDisk(i + 1)) {
			slotdata[HEADER_NEXTID + 0] = 0xFF;
			slotdata[HEADER_NEXTID + 1] = 0xFF;
		}
		else {
			slotdata[HEADER_NEXTID + 0] = (uint8_t)(slots[i + 1] >> 0);
			slotdata[HEADER_NEXTID + 1] = (uint8_t)(slots[i + 1] >> 8);
		}

		//fixup bin size
		slotdata[HEADER_SIZE + 0] = (uint8_t)(cbinsize >> 0);
		slotdata[HEADER_SIZE + 1] = (uint8_t)(cbinsize >> 8);

		//copy the filename if this is the first block
		//also setup save disk information
		if (i == 0) {
			strncpy((char*)slotdata, shortname, 240);

			//check existance of save disk
			if (disk.HasSaveDisk()) {
				slotdata[HEADER_FLAGS] |= 0x10;
				slotdata[HEADER_SAVEID] = slots[disk.GetSides() - 1];
			}
		}

		//the save disk gets handled differently
		if (disk.IsSaveDisk(i)) {
			header = (TFlashHeader*)slotdata;
			header->flags = 0x20 | 3;
			header->ownerid = slots[0];
			header->nextid = slots[0];
			header->size = MIN(binsize, 0x10000 - 256);
			memcpy(slotdata + 256, bin, binsize);
		}

		//all other disks use the compressed data
		else {
			memcpy(slotdata + 256, cbin, cbinsize);
		}

		delete[] cbin;

		//write the data to flash
		cb(user, 1, i);
		if (dev->Flash->Write(slotdata, slots[i] * SLOTSIZE, SLOTSIZE, cb, user) == false) {
			MessageBox(0, "CFdsemu::WriteFlashGD: error writing to flash", "Error", MB_OK);
			break;
		}
		if (dev->Flash->Read(verifybuf, slots[i] * SLOTSIZE, SLOTSIZE) == false) {
			MessageBox(0, "CFdsemu::WriteFlashGD: error reading back disk side for verification", "Error", MB_OK);
			printf("verify error.\n");
			ret = false;
			break;
		}
		if (memcmp(verifybuf, slotdata, SLOTSIZE) != 0) {
			char tmpstr[256];

			sprintf(tmpstr, "CFdsemu::WriteFlashGD: verification failed for slot %d", slots[i]);
			savefile("verify.buffer.dump", verifybuf, 0x10000);
			savefile("verify.slotdata.dump", slotdata, 0x10000);
			MessageBox(0, tmpstr, "Error", MB_OK);
			ret = false;
			break;
		}
	}

	//free slot buffer
	free(slotdata);
	free(verifybuf);

	return(ret);
}

//write disk image to flash
bool CFdsemu::WriteFlash(char *filename, TCallback cb, void *user)
{
	FILE *fp;
	uint8_t buf[128];
	bool ret = false;

	printf("Writing disk image '%s'...\n", filename);
	if ((fp = fopen(filename, "rb")) == 0) {
		printf("error opening file: %s\n", filename);
	}
	else if (fread(buf, 1, 128, fp) != 128) {
		printf("error reading disk image\n");
	}
	else {

		//detect fds format
		if (buf[0] == 'F' && buf[1] == 'D' && buf[2] == 'S' && buf[3] == 0x1A) {
			printf("Detected fwNES format.\n");
			ret = WriteFlashFastFDS(filename, cb, user);
		}

		else if (buf[0] == 0x01 && buf[1] == 0x2A && buf[2] == 0x4E && buf[0x38] == 0x02) {
			printf("Detected FDS format.\n");
			ret = WriteFlashFastFDS(filename, cb, user);
		}

		//detect game doctor format
		else if (buf[3] == 0x01 && buf[4] == 0x2A && buf[5] == 0x4E && buf[0x3D] == 0x02) {
			printf("Detected Game Doctor format.\n");
			ret = WriteFlashGD(filename, cb, user);
		}

		else {
			printf("unknown disk image format\n");
		}

	}
	fclose(fp);
	return(ret);
}

bool CFdsemu::WriteFlashRaw(int addr, uint8_t *buf, int len)
{
	bool ret = false;

	ret = dev->Flash->Write(buf, addr, len);
	return(ret);
}

bool CFdsemu::ReadFlash(int slot, uint8_t **buf, int *bufsize)
{
	uint8_t *data = 0;
	bool ret = false;

	//storage for entire slot's data
	data = (uint8_t*)malloc(0x10000);

	//read entire slot into the buffer
	ret = dev->Flash->Read(data, slot * 0x10000, 0x10000);

	//if there was a problem reading the data, bail
	if (ret == false) {
		free(data);
		return(false);
	}

	//now check the header to see if it is compressed
	if (data[248] & 0x80) {
		int srclen = data[240] | (data[241] << 8);
		int destlen;
		uint8_t *output;

		//allocate and zero out the buffer
		output = (uint8_t*)malloc(0x20000);
		memset(output, 0, 0x20000);

		//decompress the slot
		destlen = decompress_lz4(data + 256, output, srclen, 0x20000) + 256;

		//copy decompressed data to buf
		*buf = (uint8_t*)malloc(destlen);
		*bufsize = destlen;

		//copy uncompressed data
		memcpy(*buf + 256, output, destlen - 256);

		//use this as lead-in
		memset(*buf, 0, 256);
		free(output);
	}

	//uncompressed data
	else {
		*buf = (uint8_t*)malloc(0x10000);
		*bufsize = 0x10000;

		//clear header and use as lead-in
		memcpy(*buf, data, 0x10000);

		//clear header and use as lead-in
		memset(*buf, 0, 256);
	}

	free(data);
	return(ret);
}

bool CFdsemu::ReadFlashRaw(int addr, uint8_t **buf, int len)
{
	uint8_t *data = 0;
	bool ret = false;

	data = (uint8_t*)malloc(len);
	ret = dev->Flash->Read(data, addr, len);
	if (ret == false) {
		free(data);
	}
	else {
		*buf = data;
	}
	return(ret);
}

bool CFdsemu::Erase(int slot)
{
	TFlashHeader *headers = dev->FlashUtil->GetHeaders();
	TFlashHeader *header;
	uint8_t *buf;

	header = &headers[slot];

	//if slot has valid saveid
	if (header->flags & 0x10) {
		if (dev->Flash->EraseSlot(header->saveid) == false) {
			return(false);
		}
	}

	//if slot has valid ownerid/nextid
	if (header->flags & 0x20) {
		while (slot != 0xFFFF) {
			header = &headers[slot];
			if (dev->Flash->EraseSlot(slot) == false) {
				return(false);
			}
			slot = header->nextid;
		}
	}

	//old style slot
	else {

		if (dev->Flash->EraseSlot(slot) == false) {
			return(false);
		}
		for (uint32_t i = (slot + 1); i < dev->Slots; i++) {
			buf = headers[i].filename;
			if (buf[0] == 0xff) {          //empty
				break;
			}
			else if (buf[0] != 0) {      //filename present
				break;
			}
			else {                    //next side
				if (dev->Flash->EraseSlot(i) == false) {
					return(false);
				}
				printf(", %d", i);
			}
		}
	}

	return(true);
}
