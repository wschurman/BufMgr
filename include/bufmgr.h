
#ifndef _BUF_H
#define _BUF_H

#include "db.h"
#include "page.h"


#include "frame.h"
#include "replacer.h"


class BufMgr 
{
	private:
		int numOfFrames;

		Frame* frames;
		Replacer* replacer; // a pointer to the abstract base class of replacement policy,
		                    // can be either set to a pointer to an LRU or MRU object. 

		int FindFrame( PageID pid );
		long totalCall; //total number of pin requests 
		long totalHit; //total number of pin requests that result in misses
		long numDirtyPageWrites; //total number of dirty pages written back to disk

	public:

		BufMgr( int numOfFrames, const char* replacementPolicy);
		~BufMgr();      
		Status PinPage( PageID pid, Page*& page, bool isEmpty=false );
		Status UnpinPage( PageID pid, bool dirty=false );
		Status NewPage( PageID& firstPid, Page*& firstPage,int howMany=1 ); 
		Status FreePage( PageID pid ); 
		Status FlushPage( PageID pid );
		Status FlushAllPages();

		unsigned int GetNumOfUnpinnedFrames();

		void ResetStat();
		void PrintStat();

};


#endif // _BUF_H
