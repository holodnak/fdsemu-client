#pragma once

#include <stdint.h>
#include "DiskSide.h"

#define MAX_SIDES		16

class CDisk {
private:
protected:
	CDiskSide *sides[MAX_SIDES];
	int numsides;
public:
	CDisk();
	~CDisk();

	void FreeSides();

	bool LoadFDS(char *filename);
	bool LoadGD(char *filename);
	bool Load(char *filename);
	bool Load(CDiskSide *side);

	bool GetBin(int side, uint8_t **bin, int *binsize);

	int GetSides() {
		return(numsides);
	}

	bool HasSaveDisk() {
		for (int i = 0; i < numsides; i++) {
			if (sides[i]->GetSaveDisk())
				return(true);
		}
		return(false);
	}

	bool IsSaveDisk(int index) {
		if (index < numsides) {
			if (sides[index]->GetSaveDisk() != 0) {
				return(true);
			}
		}
		return(false);
	}

};
