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
	// TODO: add your code here
	
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
	// TODO: add your code here
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
	// TODO: add your code here
}

//Mark the frame dirty 
void Frame::DirtyIt()
{
	dirty= true;
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
	// TODO: add your code here
	return true;
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
	// TODO: add your code here
	return true;
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
	// TODO: add your code here
	return FAIL;
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
	// TODO: add your code here
	return FAIL;
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

	// TODO: add your code here
	return FAIL;
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
	// TODO: add your code here
	return false;
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
	// TODO: add your code here
	return 0;
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
	// TODO: add your code here
	return NULL;

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
	// TODO: add your code here
	return 0;
}

//--------------------------------------------------------------------
// Destructor for Frame
//
// Input   : None
// Output  : None
//--------------------------------------------------------------------

Frame::~Frame()
{
	// TODO: add your code here
}

