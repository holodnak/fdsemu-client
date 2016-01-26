#include "DiskSide.h"

CDiskSide::CDiskSide()
{
	this->fds = 0;
	this->bin = 0;
	this->raw = 0;
	this->raw03 = 0;
}

CDiskSide::~CDiskSide()
{
	if (this->fds) {
		delete this->fds;
	}
	if (this->bin) {
		delete this->bin;
	}
	if (this->raw) {
		delete this->raw;
	}
	if (this->raw03) {
		delete this->raw03;
	}
}
