#include "mru.h"

MRU::MRU() {
	this->f_no = NULL;
}

MRU::~MRU() {

}

int MRU::PickVictim() {
	return this->f_no;
}

void MRU::AddFrame(int f) {
	this->f_no = f;
}

void MRU::RemoveFrame(int f) {
	if (f == this->f_no) {
		this->f_no = 0;
	}
}