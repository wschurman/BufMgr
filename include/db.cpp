/*
 * The DB class
 * $Id
 */

//#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iomanip>

using namespace std;

#include "db.h"
#include "bufmgr.h"

#include "btfile.h"
#include "heapfile.h"

static const int bits_per_page = MAX_SPACE * 8;

static const char* dbErrMsgs[] = {
    "Database is full",         // DB_FULL
    "Duplicate file entry",     // DUPLICATE_ENTRY
    "Unix error",               // UNIX_ERROR
    "bad page number",          // BAD_PAGE_NO
    "File IO error",            // FILE_IO_ERROR
    "File not found" ,          // FILE_NOT_FOUND
    "File name too long",       // FILE_NAME_TOO_LONG
    "Negative run size",        // NEG_RUN_SIZE
};

static error_string_table dbTable( DBMGR, dbErrMsgs );

// Member functions for class DB

// ****************************************************
// Constructor for DB
// This function creates a database with the specified number of pages
// where the pagesize is default.
// It creates a UNIX file with the proper size. 

DB::DB( const char* fname, unsigned num_pgs, Status& status, bool bCatalogBTree)
: _bCatalogBTree(bCatalogBTree) {

#ifdef DEBUG 
  cout << "Creating database " << fname
       << " with pages " << num_pgs <<endl; 
#endif

    name = strcpy(new char[strlen(fname)+1],fname);

	SPACE_MAP_START = (_bCatalogBTree)? 2 : 1;

	//check whether num_pages is greater than directory page plus space map page.
	//when b tree is used for directory page, there are 3 pages needed at least.
	//otherwise, we need 2 pages at least.
	unsigned int num_meta_pages = (_bCatalogBTree)? 3 : 2;
    num_pages = (num_pgs > num_meta_pages) ? num_pgs : num_meta_pages;

    // Create the file; fail if it's already there; open it in read/write
    // mode.
	// but for this assignment, can overwrite previous minibase.db (remove O_EXCL)
    fd = _open( name, O_RDWR | O_CREAT | O_TEMPORARY | O_BINARY, 0666 );

    if ( fd < 0 ) {
        status = MINIBASE_FIRST_ERROR( DBMGR, UNIX_ERROR );
        return;
    }


    // Make the file num_pages pages long, filled with zeroes.
    char zero = 0;
    _lseek( fd, (num_pages*MINIBASE_PAGESIZE)-1, SEEK_SET );
    _write( fd, &zero, 1 );


      // Initialize space map and directory pages.

    MINIBASE_DB = this; //set the global variable to be this.

	if (!_bCatalogBTree) {
		Status      s;
		first_page* fp;

#ifdef BM_TRACE
		s = MINIBASE_BM->PinPage( 0, (Page*&)fp, true /*==empty*/,
			"*** DB admin ***" );
#else
		s = MINIBASE_BM->PinPage( 0, (Page*&)fp, true /*==empty*/ );
#endif
		if ( s != OK ) {
			status = MINIBASE_CHAIN_ERROR( DBMGR, s );
			return;
		}

		//Mingsheng 06.10.18 added new catalog management code with B+ tree

		fp->num_db_pages = num_pages;
		init_dir_page( &fp->dir, sizeof *fp );

		s = MINIBASE_BM->UnpinPage( 0 , true );
		if ( s != OK ) {
			status = MINIBASE_CHAIN_ERROR( DBMGR, s );
			return;
		}
	}
	else {
		//create the special B+ tree and store the header file in page 0.
		btree = BTreeFile::Create(status);
		heapFile = new HeapFile(status);
	}
	
    // Calculate how many pages are needed for the space map.  Reserve pages
    // 0 and 1 and as many additional pages for the space map as are needed.
    unsigned num_map_pages = (num_pages + bits_per_page - 1) / bits_per_page;
    status = set_bits( 0, SPACE_MAP_START + num_map_pages, 1 );
}

// ********************************************************
// Another constructor for DB
// This function opens an existing database in both input and output
// mode.

//DB::DB(const char* fname, Status& status)
//{
//
//#ifdef DEBUG
//    cout << "opening database "<< fname << endl;
//#endif
//
//    name = strcpy(new char[strlen(fname)+1],fname);
//
//    // Open the file in both input and output mode.
//    fd = ::open( name, O_RDWR );
//
//    if ( fd < 0 ) {
//        status = MINIBASE_FIRST_ERROR( DBMGR, UNIX_ERROR );
//        return;
//    }
//
//    MINIBASE_DB = this; //set the global variable to be this.
//
//    Status      s;
//    first_page* fp;
//
//    num_pages = 1;      // We initialize it to this. 
//                        // We will know the real size after we read page 0.
//
//#ifdef BM_TRACE
//    s = MINIBASE_BM->PinPage( 0, (Page*&)fp, false /*not empty*/,
//            "*** DB admin ***" );
//#else
//    s = MINIBASE_BM->PinPage( 0, (Page*&)fp );
//#endif
//
//    if ( s != OK ) {
//        status = MINIBASE_CHAIN_ERROR( DBMGR, s );
//        return;
//    }
//
//    num_pages = fp->num_db_pages;
//
//    s = MINIBASE_BM->UnpinPage( 0 );
//    if ( s != OK ) {
//        status = MINIBASE_CHAIN_ERROR( DBMGR, s );
//        return;
//    }
//
//    status = OK;
//}

// ****************************************************************
// Destructor
// This function closes the database.

DB::~DB()
{
#ifdef DEBUG
    cout<< "Closing database " << name << endl;
#endif
    _close( fd );
    fd = -1;
    free( name );

	if (_bCatalogBTree) {
		delete btree;
		delete heapFile;
	}
}

// *****************************************************
// This function destroys the database. That has the effect
// of deleting the UNIX file underlying the database. To ensure that
// any further accesses return errors, the file is also closed.

Status DB::Destroy()
{
#ifdef DEBUG
    cout << "Destroying the database" << endl;
#endif

    _close( fd );
    fd = -1;
    unlink( name );
    
    return OK;
}

// ********************************************************

const char* DB::GetName() const
{
    return name;
}

// ********************************************************

int DB::GetNumOfPages() const
{
    return num_pages;
}

// ********************************************************

int DB::GetPageSize() const
{
    return MINIBASE_PAGESIZE;
}

// ********************************************************
// This function allocates a run of pages.

Status DB::AllocatePage(PageID& start_page_num, int run_size_int)
{
#ifdef DEBUG
    cout << "Allocating a run of "<< run_size << " pages." << endl;
#endif

    if ( run_size_int < 0 ) { 
        cerr << "Allocating a negative run of pages.\n";
        return MINIBASE_FIRST_ERROR ( DBMGR, NEG_RUN_SIZE );
    }

    unsigned run_size = run_size_int;
    unsigned num_map_pages = (num_pages + bits_per_page - 1) / bits_per_page;
    unsigned current_run_start = 0, current_run_length = 0;


    // This loop goes over each page in the space map.
    Status status;
    for( unsigned i=0; i < num_map_pages; ++i ) {

		PageID pgid = SPACE_MAP_START + i;    // The space map starts at page #1.
          // Pin the space-map page.
        char* pg;
        status = MINIBASE_BM->PinPage( pgid, (Page*&)pg );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );


          // How many bits should we examine on this page?
        unsigned num_bits_this_page = num_pages - i*bits_per_page;
        if ( num_bits_this_page > bits_per_page )
            num_bits_this_page = bits_per_page;


          // Walk the page looking for a sequence of 0 bits of the appropriate
          // length.  The outer loop steps through the page's bytes, the inner
          // one steps through each byte's bits.
        for ( ; num_bits_this_page > 0 && current_run_length < run_size; ++pg )
            for ( unsigned mask=1;
				(mask < 256) && (num_bits_this_page > 0) && (current_run_length < run_size);
				mask <<= 1, --num_bits_this_page )
			{
				if ( *pg & mask ) 
				{
					current_run_start += current_run_length + 1;
					current_run_length = 0;
				} 
				else
					++current_run_length;
			}

          // Unpin the space-map page.
        status = MINIBASE_BM->UnpinPage( pgid );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );
    }


    if ( current_run_length >= run_size ) {

        start_page_num = current_run_start;
#ifdef DEBUG
        cout<<"Page allocated in get_free_pages:: "<< start_page_num << endl;
#endif
        return set_bits( start_page_num, run_size, 1 );
    }

    return MINIBASE_FIRST_ERROR( DBMGR, DB_FULL );
}

// **********************************************************
// This function deallocates a set of pages.  It does not ensure that the pages
// being deallocated are in fact allocated to begin with.

Status DB::DeallocatePage(PageID start_page_num, int run_size)
{
#ifdef DEBUG
    cout << "Deallocating a run of " << run_size << " pages starting at "
         << start_page_num << endl;
#endif

    if ( run_size < 0 ) {
      cerr << "Allocating a negative run of pages.\n";
      return MINIBASE_FIRST_ERROR ( DBMGR, NEG_RUN_SIZE);
    }

    return set_bits( start_page_num, run_size, 0 );
}

// ***********************************************************
// This function adds a record containing the file name and the first page
// of the file to the directory maintained in the header pages of the 
// database.

Status DB::AddFileEntry(const char* fname, PageID start_page_num)
{
#ifdef DEBUG
    cout << "Adding a file entry:  " << fname
         << " : " << start_page_num << endl;
#endif

      // Is the info kosher?
    if ( strlen(fname) >= MAX_NAME )
        return MINIBASE_FIRST_ERROR( DBMGR, FILE_NAME_TOO_LONG );
    if ((start_page_num < 0) || (start_page_num >= (int) num_pages) )
        return MINIBASE_FIRST_ERROR( DBMGR, BAD_PAGE_NO );


      // Does the file already exist?
    PageID tmp;
    if ( GetFileEntry(fname,tmp) == OK )
        return MINIBASE_FIRST_ERROR( DBMGR, DUPLICATE_ENTRY );

	Status   status;
	if (!_bCatalogBTree) {
		char    *pg = 0;
		directory_page* dp = 0;
		bool found = false;
		unsigned free_slot = 0;
		PageID hpid, nexthpid = 0;

		do {
			hpid = nexthpid;
			// Pin the header page.
			status = MINIBASE_BM->PinPage( hpid, (Page*&)pg );
			if ( status != OK )
				return MINIBASE_CHAIN_ERROR( DBMGR, status );

			// This complication is because the first page has a different
			// structure from that of subsequent pages.
			dp = (hpid == 0)? &((first_page*)pg)->dir : (directory_page*)pg;
			nexthpid = dp->next_page;

			unsigned entry = 0;
			while ( (entry < dp->num_entries)
				&& (dp->entries[entry].pagenum != INVALID_PAGE))
				++entry;

			if ( entry < dp->num_entries ) {
				free_slot = entry;
				found = true;
			} else if ( nexthpid != INVALID_PAGE ) {

				// We only unpin if we're going to continue looping.
				status = MINIBASE_BM->UnpinPage( hpid );
				if ( status != OK )
					return MINIBASE_CHAIN_ERROR( DBMGR, status );
			}
		} while ( nexthpid != INVALID_PAGE && !found );


		// Have to add a new header page if possible.
		if ( !found ) {
			status = AllocatePage( nexthpid );
			if ( status != OK ) {
				MINIBASE_BM->UnpinPage( hpid );
				return status;
			}

			// Set the next-page pointer on the previous directory page.
			dp->next_page = nexthpid;
			status = MINIBASE_BM->UnpinPage( hpid , true );
			if ( status != OK )
				return MINIBASE_CHAIN_ERROR( DBMGR, status );


			// Pin the newly-allocated directory page.
			hpid = nexthpid;
			status = MINIBASE_BM->PinPage( hpid, (Page*&)pg, true /*empty*/ );
			if ( status != OK )
				return MINIBASE_CHAIN_ERROR( DBMGR, status );

			dp = (directory_page*)pg;
			init_dir_page( dp, sizeof(directory_page) );
			free_slot = 0;
		}


		// At this point, "hpid" has the page id of the header page with the free
		// slot; "pg" points to the pinned page; "dp" has the directory_page
		// pointer; "free_slot" is the entry number in the directory where we're
		// going to put the new file entry.

		dp->entries[free_slot].pagenum = start_page_num;
		strcpy( dp->entries[free_slot].fname, fname );

		status = MINIBASE_BM->UnpinPage( hpid , true );
		if ( status != OK )
			status = MINIBASE_CHAIN_ERROR( DBMGR, status );
	}
	else {
		RecordID rid;
		//find out start page number and insert it into heap file
		heapFile->InsertRecord((char*)&start_page_num, 
			sizeof(start_page_num), rid);
		status = btree->Insert(fname, rid);
	}

    return status;
}

//store the value component of a Btree leaf entry with key = fname in foundRid,
//and return true.
//if there is no such entry, return false.
Status findRidFromFileName(BTreeFile* btree, 
						 const char* fname, 
						 RecordID& foundRid) {
	PageID foundPid;
	Status s = btree->Search(fname, foundPid);
	if (s != OK) 
	{
	 return FAIL;
	}

	if (foundPid == INVALID_PAGE)
	 return FAIL;

	//set start page appropriately
	BTLeafPage *leaf;
	PIN (foundPid, leaf);

	RecordID metaRid, curRid;
	char* curkey = new char[MAX_KEY_SIZE];
	bool found = false;
	for (s = leaf->GetFirst (metaRid, curkey, curRid);
	 s != DONE ; s = leaf->GetNext(metaRid, curkey, curRid)) {
		 if (KeyCmp((char*)curkey, fname) == 0) {
			foundRid = curRid;
			found = true;
			break;
		 }
	}
	delete[] curkey;

	UNPIN (foundPid, CLEAN);

	if (found)
		return OK;

	return FAIL;
}

// ***************************************************************
// This function deletes the file entry corresponding to the specified
// file from the directory maintained in the header pages of the 
// database.

Status DB::DeleteFileEntry(const char* fname)
{
#ifdef DEBUG
    cout << "Deleting the file entry for " << fname << endl;
#endif

    char* pg = 0;
    Status status;
    directory_page* dp = 0;
    bool found = false;
    unsigned slot = 0;
    PageID hpid, nexthpid = 0;

	if (!_bCatalogBTree) {
    do {
        hpid = nexthpid;
          // Pin the header page.
        status = MINIBASE_BM->PinPage( hpid, (Page*&)pg );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );

          // This complication is because the first page has a different
          // structure from that of subsequent pages.
        dp = (hpid == 0)? &((first_page*)pg)->dir : (directory_page*)pg;
        nexthpid = dp->next_page;

        unsigned entry = 0;
        while ((entry < dp->num_entries)
               && ((dp->entries[entry].pagenum == INVALID_PAGE)
                  || (strcmp(fname,dp->entries[entry].fname) != 0)) )
            ++entry;

        if ( entry < dp->num_entries ) {
            slot = entry;
            found = true;
          } else {
            status = MINIBASE_BM->UnpinPage( hpid );
            if ( status != OK )
                return MINIBASE_CHAIN_ERROR( DBMGR, status );
          }
    } while ( nexthpid != INVALID_PAGE && !found );


    if ( !found )   // Entry not found - nothing deleted
        return MINIBASE_FIRST_ERROR( DBMGR, FILE_NOT_FOUND );


      // Have to delete record at hpnum:slot
    dp->entries[slot].pagenum = INVALID_PAGE;

    status = MINIBASE_BM->UnpinPage( hpid , true );
    if ( status != OK )
        status = MINIBASE_CHAIN_ERROR( DBMGR, status );
	}
	else {
		//cout<<"not implemented"<<endl;
		//assert (false);

		RecordID foundRid;
		Status found = findRidFromFileName(btree, fname, foundRid);
		if (found != OK)
			return MINIBASE_FIRST_ERROR( DBMGR, FILE_NOT_FOUND );
			
		//find out start page number and insert it into heap file
		heapFile->DeleteRecord(foundRid);
		Status s = btree->Delete(fname, foundRid);
		if ( s != OK )
                return MINIBASE_CHAIN_ERROR( DBMGR, s );
	}

    return OK;
}

// ***************************************************************
// This function gets the start page number for the specified file.
// This is done by looking up the directory stored in the header pages.

Status DB::GetFileEntry(const char* fname, PageID& start_page)
{
#ifdef DEBUG
    cout << "Getting the file entry for " << fname << endl;
#endif

	if (!_bCatalogBTree) {
		char* pg = 0;
		Status status;
		directory_page* dp = 0;
		bool found = false;
		unsigned slot = 0;
		PageID hpid, nexthpid = 0;

		do {
			hpid = nexthpid;
			// Pin the header page.
			status = MINIBASE_BM->PinPage( hpid, (Page*&)pg );
			if ( status != OK )
				return MINIBASE_CHAIN_ERROR( DBMGR, status );

			if (!_bCatalogBTree) {
				// This complication is because the first page has a different
				// structure from that of subsequent pages.
				dp = (hpid == 0)? &((first_page*)pg)->dir : (directory_page*)pg;
				nexthpid = dp->next_page;

				unsigned entry = 0;
				while ((entry < dp->num_entries)
					&& ((dp->entries[entry].pagenum == INVALID_PAGE)
					|| (strcmp(fname,dp->entries[entry].fname) != 0)) )
					++entry;

				if ( entry < dp->num_entries ) {
					slot = entry;
					found = true;
				}

				status = MINIBASE_BM->UnpinPage( hpid );
				if ( status != OK )
					return MINIBASE_CHAIN_ERROR( DBMGR, status );
			}
			else {

			}

		} while ((nexthpid != INVALID_PAGE) && !found );


		if ( !found )   // Entry not found - don't post error, just fail.
			return FAIL;

		start_page = dp->entries[slot].pagenum;

	}
	else {
		RecordID foundRid;
		Status s = findRidFromFileName(btree, fname, foundRid);
		
		if (s == FAIL)   // Entry not found - don't post error, just fail.
			return FAIL;

		char data[sizeof(start_page)];
		int len;
		heapFile->GetRecord (foundRid, 
			data, 
			len);
		start_page = *(PageID*)data;
		assert (sizeof (PageID) == len);
	}

    return OK;
}

// **************************************************************
// This function reads the contents of the page into the specified
// memory area.
// The exact position in the file where reading has to start is found
// through the page number. 


Status DB::ReadPage(PageID pageno, Page* pageptr)
{
#ifdef DEBUG
    cout << "Reading page " << pageno << endl;
#endif

    if ((pageno < 0) || (pageno >= (int) num_pages))
        return MINIBASE_FIRST_ERROR( DBMGR, BAD_PAGE_NO );

    // Seek to the correct page
    if (_lseek( fd, (long)pageno*MINIBASE_PAGESIZE, SEEK_SET ) < 0 )
        return MINIBASE_FIRST_ERROR( DBMGR, UNIX_ERROR );
	
	// Read the appropriate number of bytes.
    if (_read( fd, pageptr, MINIBASE_PAGESIZE ) != MINIBASE_PAGESIZE )
        return MINIBASE_FIRST_ERROR( DBMGR, FILE_IO_ERROR );

    return OK;
}

// ******************************************************
// This function writes out the given page to disk.

Status DB::WritePage(PageID pageno, Page* pageptr)
{
#ifdef DEBUG
    cout << "Writing page " << pageno
         << " with pageptr " << pageptr << endl;
#endif    

    if ((pageno < 0) || (pageno >= (int) num_pages)) {
        cout << "Page num is " << pageno << endl;
        return MINIBASE_FIRST_ERROR( DBMGR, BAD_PAGE_NO );
    }

      // Seek to the correct page
    if (_lseek( fd, (long)pageno*MINIBASE_PAGESIZE, SEEK_SET ) < 0 )
        return MINIBASE_FIRST_ERROR( DBMGR, UNIX_ERROR );

      // Write the appropriate number of bytes.
    if (_write( fd, pageptr, MINIBASE_PAGESIZE ) != MINIBASE_PAGESIZE )
        return MINIBASE_FIRST_ERROR( DBMGR, FILE_IO_ERROR );

    return OK;
}

// *******************************************************
// The following function sets a given number of page bits in the
// space map to the given bit value.  This function is used both
// for allocating and deallocating pages in the space map.

Status DB::set_bits( PageID start_page, unsigned run_size, int bit )
{
    if ((start_page < 0) || (start_page+run_size > num_pages))
        return MINIBASE_FIRST_ERROR( DBMGR, BAD_PAGE_NO );

#ifdef DEBUG
    printf("set_bits:: space_map_before \n");
    dump_space_map();
#endif

      // Locate the run within the space map.
    int first_map_page = start_page / bits_per_page + SPACE_MAP_START;
    int last_map_page = (start_page+run_size-1) / bits_per_page + SPACE_MAP_START;
    unsigned first_bit_no = start_page % bits_per_page;


      // The outer loop goes over all space-map pages we need to touch.
    for ( PageID pgid=first_map_page; pgid <= last_map_page;
          ++pgid, first_bit_no=0 ) {

        Status status;

          // Pin the space-map page.
        char* pg;
        status = MINIBASE_BM->PinPage( pgid, (Page*&)pg );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );


          // Locate the piece of the run that fits on this page.
        unsigned first_byte_no = first_bit_no / 8;
        unsigned first_bit_offset = first_bit_no % 8;
        unsigned last_bit_no = first_bit_no + run_size - 1;

        if ( last_bit_no >= bits_per_page )
            last_bit_no = bits_per_page - 1;
        unsigned last_byte_no = last_bit_no / 8;

          // Find the start of this page's piece of the run.
        char* p   = pg + first_byte_no;
        char* end = pg + last_byte_no;

          // This loop actually flips the bits on the current page.
        for ( ; p <= end; ++p, first_bit_offset=0 ) {

            unsigned max_bits_this_byte = 8 - first_bit_offset;
            unsigned num_bits_this_byte = (run_size > max_bits_this_byte?
                                           max_bits_this_byte : run_size);
            unsigned mask = ((1 << num_bits_this_byte) - 1) << first_bit_offset;
            if ( bit ) 
                *p |= mask;
            else
                *p &= ~mask;
            run_size -= num_bits_this_byte;
        }

          // Unpin the space-map page.
        status = MINIBASE_BM->UnpinPage(pgid, true);
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );
    }


#ifdef DEBUG
    printf("set_bits:: space_map_afterwards \n");
    dump_space_map();
#endif

    return OK;
}

// *******************************************************
// Initialize a directory page.

void DB::init_dir_page( directory_page* dp, unsigned used_bytes )
{
    dp->next_page = INVALID_PAGE;
    dp->num_entries = (MAX_SPACE - used_bytes) / sizeof(file_entry);
	dp->entries = (file_entry *)malloc(sizeof(file_entry)*dp->num_entries);

    for ( unsigned index=0; index < dp->num_entries; ++index )
        dp->entries[index].pagenum = INVALID_PAGE;
}

// *******************************************************

Status DB::dump_space_map()
{
    unsigned num_map_pages = (num_pages + bits_per_page - 1) / bits_per_page;
    unsigned bit_number = 0;

      // This loop goes over each page in the space map.
    for( unsigned i=0; i < num_map_pages; ++i ) {
        PageID pgid = SPACE_MAP_START + i;    // The space map starts at page #1.

          // Pin the space-map page.
        char* pg;
        Status status;
        status = MINIBASE_BM->PinPage( pgid, (Page*&)pg );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );


          // How many bits should we examine on this page?
        unsigned num_bits_this_page = num_pages - i*bits_per_page;
        if ( num_bits_this_page > bits_per_page )
            num_bits_this_page = bits_per_page;


          // Walk the page looking for a sequence of 0 bits of the appropriate
          // length.  The outer loop steps through the page's bytes, the inner
          // one steps through each byte's bits.
        for ( ; num_bits_this_page > 0; ++pg )
            for ( unsigned mask=1;
                  mask < 256 && num_bits_this_page > 0;
                  mask <<= 1, --num_bits_this_page, ++bit_number ) {

                int bit = (*pg & mask) != 0;
                if ( bit_number % 10 == 0 )
                    if ( bit_number % 50 == 0 )
                      {
                        if ( bit_number ) cout << endl;
                        cout << setw(8) << bit_number << ": ";
                      }
                    else
                        cout << ' ';
                cout << bit;
            }


          // Unpin the space-map page.
        status = MINIBASE_BM->UnpinPage( pgid );
        if ( status != OK )
            return MINIBASE_CHAIN_ERROR( DBMGR, status );
    }

    cout << endl;
    return OK;
}

int DB::GetNumRelations() const {
	char    *pg = 0;
	Status   status;
	directory_page* dp = 0;
	bool found = false;
	unsigned free_slot = 0;
	PageID hpid, nexthpid = 0;

	int numRels = 0;
	do {
		hpid = nexthpid;
		// Pin the header page.
		status = MINIBASE_BM->PinPage( hpid, (Page*&)pg );
		if ( status != OK )
			return MINIBASE_CHAIN_ERROR( DBMGR, status );

		// This complication is because the first page has a different
		// structure from that of subsequent pages.
		dp = (hpid == 0)? &((first_page*)pg)->dir : (directory_page*)pg;
		nexthpid = dp->next_page;

		unsigned entry = 0;
		numRels += dp->num_entries;		
	} while ( nexthpid != INVALID_PAGE && !found );

	//execuion will never reach here
	return -1;
}

Status DB::DestroyCatalogBTree() {
	assert (btree);

	//	btree->PrintWhole();
	//btree->DumpStatistics();

	return btree->DestroyFile();
}

// *******************************************************
