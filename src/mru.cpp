#include "mru.h"

MRU::MRU() {
	this->first = NULL;
	this->last = NULL;
}

MRU::~MRU() {
	lFrame* cur_n = NULL;
	lFrame* cur = this->first;
	while (cur != NULL) {
		cur_n = cur->next;
		delete cur;
		cur = cur_n;
	}
	this->first = NULL;
	this->last = NULL;
}

int MRU::PickVictim() {
	lFrame* ret = this->first;
	this->first = this->first->next;
	int ret_no = ret->f_no;
	delete ret;
	return ret_no;
}

void MRU::AddFrame(int f) {
	lFrame* fr = new lFrame;
	fr->f_no = f;
	fr->next = NULL;

	if (this->first) {
		fr->next = this->first;
		this->first = fr;
	} else {
		this->first = fr;
		this->last = fr;
	}
}

void MRU::RemoveFrame(int f) {
	lFrame* prev = NULL;
	lFrame* cur = this->first;
	while (cur != NULL) {
		if (cur->f_no == f) {
			if (first == last) {
				//cout << "AHAHAHAHA" << endl;
				this->first = NULL;
				this->last = NULL;
				delete cur;
			} else if (!prev) { // deleting from head
				this->first = cur->next;
				delete cur;
			} else if (!cur->next) { // deleting tail
				this->last = prev;
				this->last->next = NULL;
				delete cur;
			} else {
				prev->next = cur->next;
				delete cur;
			}
			return;
		}
		prev = cur;
		cur = cur->next;
	}
}