#pragma once

#include <stdint.h>

class CDisk;

class CDiskSide {
	friend CDisk;
private:
protected:
	uint8_t *raw, *raw03, *bin;
	int rawsize, binsize;
	int datasize;
	int issavedisk;
public:
	CDiskSide();
	~CDiskSide();

	bool LoadFDS(uint8_t *buf, int bufsize);
	bool LoadGD(uint8_t *buf, int bufsize);

	bool Duplicate(CDiskSide *side);

	bool GetBin(uint8_t **buf, int *bufsize);

	void SetSaveDisk(int yes) {
		issavedisk = yes;
	}
	int GetSaveDisk() {
		return(issavedisk);
	}
};
