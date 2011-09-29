#ifndef _MRU_H
#define _MRU_H

#include "db.h"

#include "replacer.h"

// MRU Buffer Replacement
class MRU : public Replacer {
private:
	
	int f_no;

public:
	MRU();
	virtual ~MRU();

	virtual int PickVictim();
	virtual void AddFrame(int f);
	virtual void RemoveFrame(int f);
};

#endif // MRU