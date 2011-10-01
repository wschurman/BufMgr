#include "lru.h"

bool LRU::IsPresent(int f) {
	lFrame* cur = this->first;
	while (cur != NULL) {
		if (cur->f_no == f) {
			return true;
		}
		cur = cur->next;
	}
	return false;
}

bool LRU::IsEmpty() {
	return this->first == NULL;
}

LRU::LRU() {
	this->first = NULL;
	this->last = NULL;
}

LRU::~LRU() {

}

int LRU::PickVictim() {
	lFrame* ret = this->first;
	this->first = this->first->next;
	return ret->f_no;
}

void LRU::AddFrame(int f) {
	lFrame* fr = new lFrame;
	fr->f_no = f;

	if (this->last) {
		this->last->next = fr;
		this->last = fr;
	} else {
		this->first = fr;
		this->last = fr;
	}
}

void LRU::RemoveFrame(int f) {
	lFrame* prev = NULL;
	lFrame* cur = this->first;
	while (cur != NULL) {
		if (cur->f_no == f) {
			if (!prev) { // deleting from head
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