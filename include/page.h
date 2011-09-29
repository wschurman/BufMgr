/*
 *  Description of a simple page
 *  $Id: page.h,v 1.1 2006/09/26 23:07:08 mshong Exp $
 */

#ifndef _PAGE_H
#define _PAGE_H

#include "minirel.h"

const PageID INVALID_PAGE = -1;
const int    MAX_SPACE = MINIBASE_PAGESIZE;


class Page 
{
	private:
		
		char data[MAX_SPACE];

	public:
	    
		Page();
		~Page();
};

#endif  // _PAGE_H
