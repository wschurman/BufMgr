#ifndef _LRU_H
#define _LRU_H

#include "db.h"

#include "replacer.h"

// LRU Buffer Replacement
class LRU : public Replacer {
private:
	struct lFrame   // represents a node in a linked list
	{
		int f_no;
		lFrame* next;
	};  

	lFrame* first; 
	lFrame* last;

	bool IsPresent(int f);
	bool IsEmpty();

public:
	LRU();
	virtual ~LRU();

	virtual int PickVictim();
	virtual void AddFrame(int f);
	virtual void RemoveFrame(int f); 
};

#endif // LRU