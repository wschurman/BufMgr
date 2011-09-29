#ifndef FRAME_H
#define FRAME_H

#include "page.h"
#include "db.h"

#define INVALID_FRAME -1

class Frame 
{
	private :
	
		PageID pid;
		Page   *data;
		int    pinCount;
		bool    dirty;
		
	public :
		
		Frame();
		~Frame();
		void Pin();
		void Unpin();
		void DirtyIt();
		bool IsDirty();
		bool IsValid();
		Status Write();
		Status Read(PageID pid, bool isEmpty);
		Status Free();
		bool NotPinned();
		int GetPinCount();
		PageID GetPageID();
		Page *GetPage();

};

#endif