#include <stdio.h>
#include "FlashUtil.h"

CFlashUtil::CFlashUtil(CDevice *d)
{
	dev = d;
	headers = 0;
}

CFlashUtil::~CFlashUtil()
{
}

bool CFlashUtil::ReadHeaders()
{
	uint32_t i;

	printf("Reading flash disk headers...\n");

	//if headers already has data, free it
	if (headers) {
		delete[] headers;
	}

	//sanity check
	if (dev->Slots <= 0) {
		printf("BUG: dev->Slots is an invalid number (%d)\n", dev->Slots);
		return(false);
	}

	//allocate new chunk of data for headers
	headers = new TFlashHeader[dev->Slots];

	//loop thru all possible disk sides stored on flash
	for (i = 0; i < dev->Slots; i++) {

		//read header from flash
		if (dev->Flash->Read((uint8_t*)&headers[i], i * SLOTSIZE, FLASHHEADERSIZE) == false) {
			delete[] headers;
			headers = 0;
			printf("Error reading headers from flash.\n");
			return(false);
		}

	}

	return(true);
}

TFlashHeader *CFlashUtil::GetHeaders()
{
	if (headers == 0 && ReadHeaders() == false) {
		return(0);
	}

	return(headers);
}
