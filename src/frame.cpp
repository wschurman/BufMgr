#include "frame.h"


//--------------------------------------------------------------------
// Constructor for Frame
//
// Input   : None
// Output  : None
// PostCond: pid is set to INVALID_PAGE
//           pinCount is 0
//           dirty is false
//--------------------------------------------------------------------
Frame::Frame()
{
	this->pid = INVALID_PAGE;
	this->pinCount = 0;
	this->data = NULL;
	this->dirty = false;
}

//--------------------------------------------------------------------
// Frame::Pin
//
// Input   : None
// Output  : None
// Purpose : pin a page
//--------------------------------------------------------------------
void Frame::Pin()
{
	this->pinCount++;
}

//--------------------------------------------------------------------
// Frame::Unpin
//
// Input   : None
// Output  : None
// Purpose : unpin a page
//--------------------------------------------------------------------
void Frame::Unpin()
{
	this->pinCount--;
}

//Mark the frame dirty 
void Frame::DirtyIt()
{
	this->dirty = true;
}
//--------------------------------------------------------------------
// Frame::IsDirty
//
// Input   : None
// Output  : return true if the frame is dirty, otherwise return false
// Purpose : Check whether frame is dirty 
//--------------------------------------------------------------------

bool Frame::IsDirty()
{
	return this->dirty;
}

//--------------------------------------------------------------------
// Frame::IsValid
//
// Input   : None
// Output  : return true if the frame is storing a Page, otherwise return false
// Purpose : Check whether the frame is storing a Page
//--------------------------------------------------------------------
bool Frame::IsValid()
{
	return (this->pid != INVALID_PAGE);
}

//--------------------------------------------------------------------
// Frame::Write
//
// Input   : None
// Output  : OK if operation is successful.  FAIL otherwise.
// Purpose : Write the page to the disk
//--------------------------------------------------------------------
Status Frame::Write()
{
	Status success = MINIBASE_DB->WritePage(this->GetPageID(), this->GetPage()); // write to DB
	if (success == OK) {
		return OK;
	} else {
		return FAIL;
	}
}

//--------------------------------------------------------------------
// Frame::Read
//
// Input   : pid: page id
//           isEmpty: if true, indicate that the page is an empty page   
// Output  : OK if operation is successful.  FAIL otherwise.
// Purpose : Read the page from the disk if IsEmpty is false.
// PostCond: Instantiate the data pointer and read a page from disk if the page is not empty/
//--------------------------------------------------------------------
Status Frame::Read(PageID pid, bool isEmpty)
{	
	if (!this->data) {
		this->data = new Page(); // only create new page if data is empty
	}
	this->pid = pid;
	Status s = OK;
	if (!isEmpty) {
		 s = MINIBASE_DB->ReadPage(pid, this->data); // read from DB
	}
	return s;
}

//--------------------------------------------------------------------
// Frame::Free
//
// Input   : None
// Output  : OK if operation is successful.  FAIL otherwise.
// Purpose : Free the frame
// PostCond: pid is set to INVALID_PAGE
//           pinCount is 0
//           dirty is false
//--------------------------------------------------------------------

Status Frame::Free()
{
	this->pid = INVALID_PAGE;
	this->pinCount = 0;
	this->dirty = false;
	free(this->data);
	this->data = NULL;
	return OK;
}

//--------------------------------------------------------------------
// Frame::NotPinned
//
// Input   : None
// Output  : true if the frame is not pinned, otherwise false
// Purpose : Check if the Frame is pinned
//--------------------------------------------------------------------
bool Frame::NotPinned()
{
	return this->pinCount == 0;
}

//--------------------------------------------------------------------
// Frame::GetPageID
//
// Input   : None
// Output  : pid of the page
// Purpose : Get the pid on frame
//--------------------------------------------------------------------
PageID Frame::GetPageID()
{
	return this->pid;
}

//--------------------------------------------------------------------
// Frame::GetPage
//
// Input   : None
// Output  : data pointer to the page
// Purpose : Get the value of data pointer on frame
//--------------------------------------------------------------------

Page* Frame::GetPage()
{
	return this->data;
}

//--------------------------------------------------------------------
// Frame::GetPinCount
//
// Input   : None
// Output  : pincount of the page
// Purpose : Get the the pincount
//--------------------------------------------------------------------

int Frame::GetPinCount()
{
	return this->pinCount;
}

//--------------------------------------------------------------------
// Destructor for Frame
//
// Input   : None
// Output  : None
//--------------------------------------------------------------------

Frame::~Frame()
{
	this->Free();
	delete this->data;
	this->data = NULL;
}

