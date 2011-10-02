
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
	this->ResetStat();
	this->numOfFrames = numOfFrames;
	cout << "NUMOFFRAMES: " << numOfFrames << endl;
	this->numUnpinnedFrames = numOfFrames;
	cout << replacementPolicy << endl;
	if (replacementPolicy[0] == 'L') {
		this->replacer = new LRU();
	} else {
		this->replacer = new MRU();
	}
	
	this->frames = new Frame[numOfFrames];
	for (int i=0; i < numOfFrames; i++) {
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
	this->FlushAllPages(); // maybe
	delete [] this->frames;
	this->frames = NULL;
	delete this->replacer;
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
	if (frame_num == INVALID_PAGE) {
		//cout << "PinPage didn't find frame with pid " << pid << endl;
		if (this->GetNumOfUnpinnedFrames() == 0) {
			cout << "AHAHAHAHAHAHAHHAHAHAHHAHAH FAAIAIIILLLLL" << endl;
			page = NULL;
			return FAIL;
		}

		frame_num = this->FindFreeFrameOrReplace();
		//cout << "\nPinPage putting in frame:num: " << frame_num << endl;
		this->frames[frame_num].Read(pid, isEmpty);
	} else {
		this->totalHit++;
	}

	page = this->frames[frame_num].GetPage();
	this->frames[frame_num].Pin();
	if (this->frames[frame_num].GetPinCount() == 1) {
		//cout << "PIN PAGE decrement numupf" << endl;
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
	int frame_num = this->FindFrame(pid);
	if (frame_num == INVALID_PAGE) return FAIL;
	if (this->frames[frame_num].NotPinned()) return FAIL;
	this->frames[frame_num].Unpin();
	//cout << "UNPIN PAGE " << pid << " IN FRAME " << frame_num << " PIN COUNT: " << this->frames[frame_num].GetPinCount() << endl;
	if (dirty) this->frames[frame_num].DirtyIt();
	if (this->frames[frame_num].GetPinCount() == 0) {
		this->numUnpinnedFrames++;
		//cout << "UNPIN PAGE NUMUNPF: " << this->numUnpinnedFrames << endl;
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
	//cout << "Call to NewPage" << endl;
	if (howMany < 1) return FAIL;
	if (this->GetNumOfUnpinnedFrames() < 1) return FAIL;
	MINIBASE_DB->AllocatePage(firstPid, howMany);
	
	int frame_num = this->FindFreeFrameOrReplace();
	if (frame_num == -1) {
		cout << "AHAHAHAHAHAHA" << endl;
		return FAIL;
	}
	//cout << "NewPage, frame_num: " << frame_num << endl;
	this->frames[frame_num].Read(firstPid, true); //maybe false
	firstPage = this->frames[frame_num].GetPage();

	this->frames[frame_num].Pin();
	this->numUnpinnedFrames--;

	//cout << "PID: " << firstPid << " put in FrAME: " <<frame_num << endl;

	// hmmmmm maybe don't need
	//this->replacer->RemoveFrame(frame_num);
	//this->replacer->AddFrame(frame_num);

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
	if (frame_num != INVALID_PAGE) {
		if(this->frames[frame_num].GetPinCount() > 1) return FAIL;
		this->frames[frame_num].Unpin();
		this->numUnpinnedFrames--;
		this->frames[frame_num].Free();
		this->replacer->RemoveFrame(frame_num);
	}
	MINIBASE_DB->DeallocatePage(pid);
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
	int frame_num = this->FindFrame(pid);
	if (frame_num == INVALID_PAGE) return FAIL;
	if (!this->frames[frame_num].NotPinned()) return FAIL;

	// write to disk if dirty
	if (this->frames[frame_num].IsDirty()) {
		MINIBASE_DB->WritePage(this->frames[frame_num].GetPageID(), this->frames[frame_num].GetPage());
		this->numDirtyPageWrites++;
	}
	this->frames[frame_num].Free();
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
	for (int i = 0; i < this->numOfFrames; i++) {
		if (!this->frames[i].NotPinned()) return FAIL;
		if (this->frames[i].IsDirty()) {
			MINIBASE_DB->WritePage(this->frames[i].GetPageID(), this->frames[i].GetPage());
			this->numDirtyPageWrites++;
		}
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
	return this->numUnpinnedFrames;

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
	int n = -1;
	for(int i = 0; i < this->numOfFrames; i++) {
		if (this->frames[i].GetPageID() == pid) n = i;
	}
	if (n != -1) {
		this->replacer->RemoveFrame(n);
		this->replacer->AddFrame(n);
		return n;
	}
	//cout << "FIND FRAME DID NOT FIND FRAME: " << pid << "AHAHA" << n << endl;
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
	//cout << "CAll to FFFOR" << endl;
	//cout << "NUPF:" << this->numUnpinnedFrames << endl;
	if (this->numUnpinnedFrames == 0) return -1;
	// find a free frame
	for(int i = 0; i < this->numOfFrames; i++) {
		if (!this->frames[i].IsValid()) {
			//cout << "RETURNING " << i << endl;
			return i;
		}
	}
	
	// no free frames
	int n = this->replacer->PickVictim();
	//cout << "FFFOR: didn't find free frame, flushing " << n << endl;
	Status s = this->FlushPage(this->frames[n].GetPageID());
	if (s == OK) this->frames[n].Free();
	else return -1;
	return n;
}
