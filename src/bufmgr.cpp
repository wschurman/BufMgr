#include "bufmgr.h"
#include "frame.h"
#include "replacer.h"
#include "lru.h"
#include "mru.h"

//--------------------------------------------------------------------
// Constructor for BufMgr
//
// Input   : numOfFrames  - number of frames(pages) in the this buffer manager
//           replacementPolicy - a replacement policy, either LRU or MRU            
// Output  : None
// PostCond: All frames are empty.
//           the "replacer" is initiated to LRU or MRU according to the
//           replacement policy.
//--------------------------------------------------------------------

BufMgr::BufMgr( int numOfFrames, const char* replacementPolicy)
{
	cout << "** It seems that some of the code outside of\n** BufMgr class is trying to write to heap memory that \n** belongs to the BufMgr class." << endl;
	cout << "** Our workaround is to comment out the \n** 'delete minibase_globals' (main.cpp:42) or run in Release mode.\n" << endl;

	this->ResetStat();
	this->numOfFrames = numOfFrames;
	this->numUnpinnedFrames = numOfFrames;

	cout << "Replacement Policy: " << replacementPolicy << endl;
	if (replacementPolicy[0] == 'L') { // set replacement policy
		this->replacer = new LRU();
	} else {
		this->replacer = new MRU();
	}
	
	// allocate frames
	this->frames = new Frame[numOfFrames];
	for (int i=0; i < numOfFrames; i++) { // free all frames
		this->frames[i].Free();
	}
}


//--------------------------------------------------------------------
// Destructor for BufMgr
//
// Input   : None
// Output  : None
//--------------------------------------------------------------------

BufMgr::~BufMgr()
{   
	// fluch pages and delete all allocated memory
	this->FlushAllPages();
	this->numOfFrames = 0;
	delete[] this->frames;
	this->frames =   NULL;
	delete this->replacer;
	this->replacer = NULL;
}

//--------------------------------------------------------------------
// BufMgr::PinPage
//
// Input    : pid     - page id of a particular page 
//            isEmpty - (optional, default to false) if true indicate
//                      that the page to be pinned is an empty page.
// Output   : page - a pointer to a page in the buffer pool. (NULL
//            if fail)
// Purpose  : Pin the page with page id = pid to the buffer.  
//            Read the page from disk unless isEmpty is true or unless
//            the page is already in the buffer.
// Condition: Either the page is already in the buffer, or there is at
//            least one frame available in the buffer pool for the 
//            page.
// PostCond : The page with page id = pid resides in the buffer and 
//            is pinned. The number of pin on the page increase by
//            one.
// Return   : OK if operation is successful.  FAIL otherwise.
//--------------------------------------------------------------------


Status BufMgr::PinPage(PageID pid, Page*& page, bool isEmpty)
{
	int frame_num = this->FindFrame(pid);
	if (frame_num == INVALID_PAGE) { // frame is not pinned to the buffer
		if (this->GetNumOfUnpinnedFrames() == 0) { // no space for new page
			page = NULL;
			return FAIL;
		}

		frame_num = this->FindFreeFrameOrReplace();
		this->frames[frame_num].Read(pid, isEmpty); // read in new frame
	} else { // frame in buffer, so hit
		this->totalHit++;
	}

	page = this->frames[frame_num].GetPage(); // set page
	this->frames[frame_num].Pin();
	if (this->frames[frame_num].GetPinCount() == 1) { // first pin, remove from replacer and count unpinned frames
		this->replacer->RemoveFrame(frame_num);
		this->numUnpinnedFrames--;
	}
	this->totalCall++;

	return OK;
} 

//--------------------------------------------------------------------
// BufMgr::UnpinPage
//
// Input    : pid     - page id of a particular page 
//            dirty   - indicate whether the page with page id = pid
//                      is dirty or not. (Optional, default to false)
// Output   : None
// Purpose  : Unpin the page with page id = pid in the buffer. Mark 
//            the page dirty if dirty is true.  
// Condition: The page is already in the buffer and is pinned.
// PostCond : The page is unpinned and the number of pin on the
//            page decrease by one. 
// Return   : OK if operation is successful.  FAIL otherwise.
//--------------------------------------------------------------------


Status BufMgr::UnpinPage(PageID pid, bool dirty)
{
	// conditions
	int frame_num = this->FindFrame(pid);
	if (frame_num == INVALID_PAGE) return FAIL;
	if (this->frames[frame_num].NotPinned()) return FAIL;

	// unpin and dirty if necessary
	this->frames[frame_num].Unpin();
	if (dirty) this->frames[frame_num].DirtyIt();
	if (this->frames[frame_num].GetPinCount() == 0) { // just unpinned completely, so add to replacer
		this->numUnpinnedFrames++;
		this->replacer->RemoveFrame(frame_num);
		this->replacer->AddFrame(frame_num);
	}
	return OK;
}

//--------------------------------------------------------------------
// BufMgr::NewPage
//
// Input    : howMany - (optional, default to 1) how many pages to 
//                      allocate.
// Output   : firstPid  - the page id of the first page (as output by
//                   DB::AllocatePage) allocated.
//            firstPage - a pointer to the page in memory.
// Purpose  : Allocate howMany number of pages, and pin the first page
//            into the buffer. 
// Condition: howMany > 0 and there is at least one free buffer space
//            to hold a page.
// PostCond : The page with page id = pid is pinned into the buffer.
// Return   : OK if operation is successful.  FAIL otherwise.
// Note     : You can call DB::AllocatePage() to allocate a page.  
//            You should call DB:DeallocatePage() to deallocate the
//            pages you allocate if you failed to pin the page in the
//            buffer.
//--------------------------------------------------------------------


Status BufMgr::NewPage (PageID& firstPid, Page*& firstPage, int howMany)
{
	if (howMany < 1) return FAIL;
	if (this->GetNumOfUnpinnedFrames() < 1) return FAIL;
	MINIBASE_DB->AllocatePage(firstPid, howMany); // allocate one or more pages in DB
	
	int frame_num = this->FindFreeFrameOrReplace();
	if (frame_num == -1) {
		return FAIL; // no free frames
	}
	this->frames[frame_num].Read(firstPid, true); // read into the chosen slot
	firstPage = this->frames[frame_num].GetPage(); // return pointer

	this->frames[frame_num].Pin();
	this->numUnpinnedFrames--; // one less pinned frame

	return OK;
}

//--------------------------------------------------------------------
// BufMgr::FreePage
//
// Input    : pid     - page id of a particular page 
// Output   : None
// Purpose  : Free the memory allocated for the page with 
//            page id = pid  
// Condition: Either the page is already in the buffer and is pinned
//            no more than once, or the page is not in the buffer.
// PostCond : The page is unpinned, and the frame where it resides in
//            the buffer pool is freed.  Also the page is deallocated
//            from the database. 
// Return   : OK if operation is successful.  FAIL otherwise.
// Note     : You can call MINIBASE_DB->DeallocatePage(pid) to
//            deallocate a page.
//--------------------------------------------------------------------


Status BufMgr::FreePage(PageID pid)
{
	int frame_num = this->FindFrame(pid);
	if (frame_num != INVALID_PAGE) { // remove pin, free, remove from replacer
		if(this->frames[frame_num].GetPinCount() > 1) return FAIL;
		this->frames[frame_num].Unpin();
		this->numUnpinnedFrames--;
		this->frames[frame_num].Free();
		this->replacer->RemoveFrame(frame_num);
	}
	MINIBASE_DB->DeallocatePage(pid); // deallocate in DB
	return OK;
}


//--------------------------------------------------------------------
// BufMgr::FlushPage
//
// Input    : pid  - page id of a particular page 
// Output   : None
// Purpose  : Flush the page with the given pid to disk.
// Condition: The page with page id = pid must be in the buffer,
//            and is not pinned. pid cannot be INVALID_PAGE.
// PostCond : The page with page id = pid is written to disk if it's dirty. 
//            The frame where the page resides is empty.
// Return   : OK if operation is successful.  FAIL otherwise.
//--------------------------------------------------------------------


Status BufMgr::FlushPage(PageID pid)
{
	// conditions
	int frame_num = this->FindFrame(pid);
	if (frame_num == INVALID_PAGE) return FAIL;
	if (!this->frames[frame_num].NotPinned()) return FAIL;

	// write to disk if dirty
	if (this->frames[frame_num].IsDirty()) {
		this->frames[frame_num].Write();
		this->numDirtyPageWrites++;
	}
	this->frames[frame_num].Free(); // free the page frame
	this->replacer->RemoveFrame(frame_num);
	return OK;
} 

//--------------------------------------------------------------------
// BufMgr::FlushAllPages
//
// Input    : None
// Output   : None
// Purpose  : Flush all pages in this buffer pool to disk.
// Condition: All pages in the buffer pool must not be pinned.
// PostCond : All dirty pages in the buffer pool are written to 
//            disk (even if some pages are pinned). All frames are empty.
// Return   : OK if operation is successful.  FAIL otherwise.
//--------------------------------------------------------------------

Status BufMgr::FlushAllPages()
{
	// efficient instead of O(n^2) lookups if made calls to FlushPage for each i
	for (int i = 0; i < this->numOfFrames; i++) {
		if (!this->frames[i].NotPinned()) return FAIL;
		if (this->frames[i].IsDirty()) {
			this->frames[i].Write();
			this->numDirtyPageWrites++;
		}
		// free and remove from replacer
		this->frames[i].Free();
		this->replacer->RemoveFrame(i);
	}
	return OK;
}


//--------------------------------------------------------------------
// BufMgr::GetNumOfUnpinnedFrames
//
// Input    : None
// Output   : None
// Purpose  : Find out how many unpinned locations are in the buffer
//            pool.
// Condition: None
// PostCond : None
// Return   : The number of unpinned buffers in the buffer pool.
//--------------------------------------------------------------------

unsigned int BufMgr::GetNumOfUnpinnedFrames()
{
	return this->numUnpinnedFrames; // O(1)
}

void BufMgr::ResetStat() { 
	totalHit = 0; 
	totalCall = 0; 
	numDirtyPageWrites = 0;
}

void  BufMgr::PrintStat() {
	cout<<"**Buffer Manager Statistics**"<<endl;
	cout<<"Number of Dirty Pages Written to Disk: "<<numDirtyPageWrites<<endl;
	cout<<"Number of Pin Page Requests: "<<totalCall<<endl;
	cout<<"Number of Pin Page Request Misses "<<totalCall-totalHit<<endl;
}

//--------------------------------------------------------------------
// BufMgr::FindFrame
//
// Input    : pid - a page id 
// Output   : None
// Purpose  : Look for the page in the buffer pool, return the frame
//            number if found.
// PreCond  : None
// PostCond : None
// Return   : the frame number if found. INVALID_FRAME otherwise.
//--------------------------------------------------------------------

int BufMgr::FindFrame( PageID pid )
{
	// iterate through frames until the correct one is found
	int n = -1;
	for(int i = 0; i < this->numOfFrames; i++) {
		if (this->frames[i].GetPageID() == pid) n = i;
	}
	if (n != -1) { // put in replacer because it was accessed recently
		if (this->frames[n].NotPinned()) {
			this->replacer->RemoveFrame(n);
			this->replacer->AddFrame(n);
		}
		return n;
	}
	return INVALID_FRAME;
}

//--------------------------------------------------------------------
// BufMgr::FindFreeFrameOrReplace
//
// Input    : None
// Output   : frame number of the free frame
// Purpose  : Find a free frame in the buffer. If none exists, choose
//			  a victim frame according to the buffer replacement
//			  policy, and write the current page in that frame back to 
//			  disk if the frame holds a page and that page is dirty.
// Condition: None
// PostCond : a free frame at the returned frame number exists 
// Return   : frame number of free frame. -1 if no unpinned frames
//--------------------------------------------------------------------

int BufMgr::FindFreeFrameOrReplace() {
	if (this->numUnpinnedFrames == 0) return -1;
	// find a free frame
	for(int i = 0; i < this->numOfFrames; i++) {
		if (!this->frames[i].IsValid()) {
			return i;
		}
	}
	
	// no free frames, pick one from replacer, Free it, and return the frame numer
	int n = this->replacer->PickVictim();
	Status s = this->FlushPage(this->frames[n].GetPageID());
	if (s == OK) this->frames[n].Free();
	else return -1;
	return n;
}
