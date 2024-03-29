#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <assert.h>
#include <conio.h>
#include <time.h>
#include "bufmgr.h"
#include "db.h"
#include "bmtest.h"

using namespace std;

BMTester::BMTester() : TestDriver( "buftest" )
{

}


BMTester::~BMTester()
{

}


int BMTester::Test1()
{
	unsigned numPages;
	Page* pg;
	PageID pid, firstPid, lastPid;
	Status status;
	int data;

	cout << "\n  Test 1 does a simple test of normal buffer manager operations:\n";

	numPages = MINIBASE_BM->GetNumOfUnpinnedFrames() + 1; // to ensure > 1 page is written

	//
	// Allocating Pages
	//
	cout << "  - Allocate a bunch of new pages\n";
	status = MINIBASE_BM->NewPage( firstPid, pg, numPages );
	if ( status != OK )
	{
		cerr << "*** Could not allocate " << numPages << " new pages in the database.\n";
		return false;
	}

	
	//PageID tempPid;

	//status = MINIBASE_BM->NewPage( tempPid, pg, numPages );

	
	//
	// Unpin that first page... to simplify our loop.
	//
	status = MINIBASE_BM->UnpinPage(firstPid);
	if ( status != OK )
		cerr << "*** Could not unpin the first new page.\n";

	//
	// Write something to each page
	//
	cout << "  - Write something on each one\n";
	for ( pid=firstPid, lastPid=pid+numPages; status == OK && pid < lastPid; ++pid )
	{
		status = MINIBASE_BM->PinPage( pid, pg, true ); // true indicate empty page
		if ( status != OK )
	   		cerr << "*** Could not pin new page " << pid << endl;

		if ( status == OK )
		{
			// Copy the page number + 99999 onto each page.  It seems
			// unlikely that this bit pattern would show up there by
			// coincidence.
			data = pid + 99999;
			memcpy( (void*)pg, &data, sizeof (int) );  
			status = MINIBASE_BM->UnpinPage( pid , true);
			if ( status != OK )
				cerr << "*** Could not unpin dirty page " << pid << endl;
		}
	}


	//
	// Read something back from each one.
	//
	if ( status == OK )
		cout << "  - Read that something back from each one\n"
			 << "    (because we're buffering, this is where most of the writes happen)\n";

	for ( pid=firstPid; status == OK && pid < lastPid; ++pid )
	{  
		status = MINIBASE_BM->PinPage( pid, pg );
		if ( status != OK )
		{
			cerr << "*** Could not pin page " << pid << endl;
		}
		else if ( status == OK )
		{
            memcpy( &data, (void*)pg, sizeof data );
            if ( data != pid + 99999 )
            {
                status = FAIL;
                cerr << "*** Read wrong data back from page " << pid << endl;
            }

            status = MINIBASE_BM->UnpinPage( pid , true );
            if ( status != OK )
                cerr << "*** Could not unpin page " << pid << endl;
        }
    }

    if ( status == OK ) 
		cout << "  - Free the pages again\n";

    for ( pid=firstPid; pid < lastPid; ++pid )
    {
        Status freeStatus = MINIBASE_BM->FreePage( pid );
        if ( status == OK && freeStatus != OK )
        {
            status = freeStatus;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }

    if ( status == OK )
        cout << "  Test 1 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();

    return status == OK;
}



int BMTester::Test2()
{
    cout << "\n  Test 2 exercises some illegal buffer manager operations:\n";

    minibase_errors.clear_errors();

    // We choose this number to ensure that pinning this number of buffers
    // should fail.
    unsigned numPages = MINIBASE_BM->GetNumOfUnpinnedFrames() + 1;
    Page* pg;
    PageID pid, firstPid, lastPid;


    cout << "  - Try to pin more pages than there are frames\n";
    Status status = MINIBASE_BM->NewPage( firstPid, pg, numPages );
    if ( status != OK )
    {
        cerr << "*** Could not allocate " << numPages << " new pages in the database.\n";
        return false;
    }

    // First pin enough pages that there is no more room.
    for ( pid=firstPid+1, lastPid=firstPid+numPages-1;
          status == OK && pid < lastPid; ++pid )
    {
        status = MINIBASE_BM->PinPage( pid, pg, true );
        if ( status != OK )
            cerr << "*** Could not pin new page " << pid << endl;
    }

    // Make sure the buffer manager thinks there's no more room.
    if ( status == OK  &&  MINIBASE_BM->GetNumOfUnpinnedFrames() != 0 )
    {
        status = FAIL;
        cerr << "*** The buffer manager thinks it has "
             << MINIBASE_BM->GetNumOfUnpinnedFrames() << " available frames,\n"
             << "    but it should have none.\n";
    }

    // Now pin that last page, and make sure it fails.
    if ( status == OK )
    {
        status = MINIBASE_BM->PinPage( lastPid, pg,  true );
        TestFailure( status, FAIL, "Pinning too many pages" );
    }

    if ( status == OK )
    {
        status = MINIBASE_BM->PinPage( firstPid, pg );
        if ( status != OK )
            cerr << "*** Could not acquire a second pin on a page\n";
    }

    if ( status == OK )
    {
        cout << "  - Try to free a doubly-pinned page\n";
        status = MINIBASE_BM->FreePage( firstPid );
        TestFailure( status, FAIL, "Freeing a pinned page" );
        MINIBASE_BM->UnpinPage( firstPid );
    }

    if ( status == OK )
    {
        cout << "  - Try to unpin an unpinned page\n";
        status = MINIBASE_BM->UnpinPage( lastPid );
        TestFailure( status, FAIL, "Unpinning an unpinned page" );
    }

    for ( pid=firstPid; pid <= lastPid; ++pid )
    {
        Status st2 = MINIBASE_BM->FreePage( pid );
        if ( status == OK && st2 != OK )
        {
            status = st2;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }



    if ( status == OK )
        cout << "  Test 2 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();

    return status == OK;
	//	return true;
}


int BMTester::Test3()
{
    cout << "\n  Test 3 exercises some of the internals of the buffer manager\n";

    unsigned index, numPages = NUMBUF + 10;
    Page* pg;
    PageID pid, pids[NUMBUF+10];
    Status status = OK;

    cout << "  - Allocate and dirty some new pages, one at a time, and leave some pinned\n";
    for ( index=0; status == OK && index < numPages; ++index )
    {
        status = MINIBASE_BM->NewPage( pid, pg );
        if ( status == OK )
            pids[index] = pid;
        else
            cerr << "*** Could not allocate new page number " << index+1 << endl;

        if ( status == OK )
        {
            // Copy the page number + 99999 onto each page.  It seems
            // unlikely that this bit pattern would show up there by
            // coincidence.
            int data = pid + 99999;
            memcpy( (void*)pg, &data, sizeof data );
			
            // Leave the page pinned if it equals 12 mod 20.  This is a
            // random number based loosely on a bug report.
            if ( pid % 20 != 12 )
                status = MINIBASE_BM->UnpinPage( pid , true );
            if ( status != OK )
                cerr << "*** Could not unpin dirty page " << pid << endl;
        }
    }


    if ( status == OK )
        cout << "  - Read the pages\n";

    for ( index=0; status == OK && index < numPages; ++index )
    {
        pid = pids[index];
        status = MINIBASE_BM->PinPage( pid, pg );
        if ( status != OK )
            cerr << "*** Could not pin page " << pid << endl;

        if ( status == OK )
        {
            int data;
            memcpy( &data, (void*)pg, sizeof data );

            if ( data != pid + 99999 )
            {
                status = FAIL;
                cerr << "*** Read wrong data back from page " << pid << endl;
            }
        }
        if ( status == OK )
        {
            status = MINIBASE_BM->UnpinPage( pid , true );
            if ( status != OK )
                cerr << "*** Could not unpin page " << pid << endl;
        }
        if ( status == OK && (pid % 20 == 12) )
        {
            status = MINIBASE_BM->UnpinPage( pid , true );
            if ( status != OK )
                cerr << "*** Could not unpin page " << pid << endl;
        }
    }

	for ( index=0; index < numPages; index++ )
    {
		pid = pids[index];

        Status st2 = MINIBASE_BM->FreePage( pid );
        if ( status == OK && st2 != OK )
        {
            status = st2;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }

    if ( status == OK )
        cout << "  Test 3 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();


    return status == OK;
	//return true;
}

int BMTester::Test4()
{	//
	//  A test on relation between buffer size and pinpage request miss/hit.
	//
	Page* pg;
	PageID pid;
	Status status;
	int data, times;
	clock_t initTime, endTime;

	cout << "\n  Test 4 tests relation between buffer size and pinpage request miss/hit:\n";

	// The number of pages will be allocated for this test
	const int numPages = 200; 

	// An array to save the pageID of the pages allocated
	PageID pids[numPages];

	// The number of times to access those above allocated pages
	// (Simulate the usage of page in page nested loop join)
	times = 12;	

	//
	// Allocating Pages
	//
	
	// Record the initial running time 
	initTime = clock();

	// Start to collect statistics
	MINIBASE_BM->ResetStat();

	//
	// Write something to each page
	//
	for ( int index=0; index<numPages; index++ )
	{
		// Allocate page and pin it into buffer pool
		status = MINIBASE_BM->NewPage( pid, pg );
        if ( status != OK ){
            cerr << "*** Could not allocate new page number " << index+1 << endl;
			return FAIL;
		}


		pids[index] = pid;
		if ( status == OK )
		{
			
			// Copy the page number + 99999 onto each page.  It seems
			// unlikely that this bit pattern would show up there by
			// coincidence.
			data = pid + 99999;
			memcpy( (void*)pg, &data, sizeof data );

			status = MINIBASE_BM->UnpinPage( pid , true);
			if ( status != OK )
				cerr << "*** Could not unpin dirty page " << pid << endl;
		}
	}
	
	

	
	cout << "  - Read in " << numPages << " pages " << times << " times into buffer pool\n";
	cout << "  - Collect the Statistics\n";
	for (int loop=0; loop<times; loop++){

		for ( int i=0; i < numPages; i++ )
		{  
			pid = pids[i];
			status = MINIBASE_BM->PinPage( pid, pg );
			if ( status != OK )
			{
				cerr << "*** Could not pin page " << pid << endl;
			}
			else if ( status == OK )
			{
				/*
				// change something to the page			
				memcpy( &data, (void*)pg, sizeof data );
				data++;
				memcpy( (void*)pg, &data, sizeof data );
				*/
				status = MINIBASE_BM->UnpinPage( pid ,true);
				if ( status != OK )
					cerr << "*** Could not unpin page " << pid << endl;
			}
		}

	}
	
	MINIBASE_BM->FlushAllPages();

    				     
	// record the end time
	endTime = clock();

    for (int index=0; index < numPages; index++ )
    {
		pid = pids[index];

        Status freeStatus;
		
		freeStatus= MINIBASE_BM->FreePage( pid );
        if ( status == OK && freeStatus != OK )
        {
            status = freeStatus;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }

	
	cout << "  - The running time for above operation is " << (endTime - initTime)*(1000.0/CLOCKS_PER_SEC) <<"ms\n";
	
	cout << "  - Starting to print Statistics \n";
	// Start to print statistics
	MINIBASE_BM->PrintStat();

    if ( status == OK )
        cout << "  Test 4 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();

    return  true;
	    
}

int BMTester::Test5()
{
	//
	//  A test on locality.
	//
	Page* pg;
	PageID pid;
	Status status;
	int data, times;
	clock_t initTime, endTime;

	cout << "\n  Test 5 tests locality\n";

	// The number of pages will be allocated for this test
	const int numPages = 400; 

	// An array to save the pageID of the pages allocated
	PageID pids[numPages];

	// The number of times to access those above allocated pages
	// (Simulate the usage of page in page nested loop join)

	times = 5;	

	//
	// Allocating Pages
	//
	
	// Record the initial running time 
	initTime = clock();

	// Start to collect statistics
	MINIBASE_BM->ResetStat();
	
	//
	// Write something to each page
	//
	for ( int index=0; index<numPages; index++ )
	{
		// Allocate page and pin it into buffer pool
		status = MINIBASE_BM->NewPage( pid, pg );
        if ( status != OK ){
            cerr << "*** Could not allocate new page number " << index+1 << endl;
			return FAIL;
		}


		pids[index] = pid;
		if ( status == OK )
		{
			
			// Copy the page number + 99999 onto each page.  It seems
			// unlikely that this bit pattern would show up there by
			// coincidence.
			data = pid + 99999;
			memcpy( (void*)pg, &data, sizeof data );

			status = MINIBASE_BM->UnpinPage( pid , true);
			if ( status != OK )
				cerr << "*** Could not unpin dirty page " << pid << endl;
		}
	}
	
	cout << "  - Read in " << numPages << " pages " << times << " times into buffer pool\n";
	cout << "  - Collect the Statistics\n";

	int range;

	for (int loop=0; loop<times; loop++){

		if(loop == 0) 
			range = numPages;
		else
			range = numPages/5;

		for ( int i=0; i < range; i++ )
		{  
			pid = pids[i];
			status = MINIBASE_BM->PinPage( pid, pg );
			if ( status != OK )
			{
				cerr << "*** Could not pin page " << pid << endl;
			}
			else if ( status == OK )
			{
				// change something to the page			
				memcpy( &data, (void*)pg, sizeof data );
				data++;
				memcpy( (void*)pg, &data, sizeof data );
				
				status = MINIBASE_BM->UnpinPage( pid , true );
				if ( status != OK )
					cerr << "*** Could not unpin page " << pid << endl;
			}
		}

	}

	MINIBASE_BM->FlushAllPages();
    				     
	// record the end time
	endTime = clock();

    for (int index=0; index < numPages; index++ )
    {
		pid = pids[index];

        Status freeStatus = MINIBASE_BM->FreePage( pid );
        if ( status == OK && freeStatus != OK )
        {
            status = freeStatus;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }


	cout << "  - The running time for above operation is " << (endTime - initTime)*(1000.0/CLOCKS_PER_SEC) <<"ms\n";
	
	cout << "  - Starting to print Statistics \n";
	// Start to print statistics
	MINIBASE_BM->PrintStat();
	
    cout << "  Test 5 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();
    return true;
}

/**
 * This test case simulates the usage of pages in a particular workload in which 
 * an LRU replacement policy works better than an MRU replacement policy.
 */
int BMTester::Test6()
{
	// This test allocates 2 pages, pinning and unpinning them, and then gets the pages
	// again and then increments the value in it, causing misses if the pages were not
	// in the buffer. LRU works best since neither of the two pages should be removed
	// from the buffer before they are incremented since there are older pages to remove.
	// MRU performs worse since when the buffer is full it removes a page after its created
	// to make room for a new page before it is needed again for incrementing.

	Page* pg;
	PageID pid;
	Status status;
	int data;
	clock_t initTime, endTime;

	cout << "\n  Test 6 simulates an LRU-favorabe workload\n";

	// The number of pages will be allocated for this test
	const int numPages = 100; 

	// An array to save the pageID of the pages allocated
	PageID pids[numPages];

	//
	// Allocating Pages
	//
	
	// Record the initial running time 
	initTime = clock();

	// Start to collect statistics
	MINIBASE_BM->ResetStat();
	
	//
	// Write something to each page
	// and then increment the value in the page

	
	cout << "  - Allocate 2 Pages and increment their value by increaser variable \n";
	cout << "       before moving on to the next 2 pages \n";

	int increaser = 5;
	int index;
	for ( int i=0; i<numPages/2; i++ )
	{
		for ( int k=0; k<2; k++)
		{
			index= 2*i+k;
			// Allocate page and pin it into buffer pool
			status = MINIBASE_BM->NewPage( pid, pg );
			if ( status != OK ){
				cerr << "*** Could not allocate new page number " << index+1 << endl;
				return FAIL;
			}
	
			pids[index] = pid;
			if ( status == OK )
			{
			
				// Copy the page number + 99999 onto each page.  
				data = pid + 99999;
				memcpy( (void*)pg, &data, sizeof data );


				status = MINIBASE_BM->UnpinPage( pid , true);
				if ( status != OK )
					cerr << "*** Could not unpin dirty page " << pid << endl;
			}
		}

		//increment
		for (int j=0; j<2; j++)
		{
			pid=pids[2*i+j];
			status = MINIBASE_BM->PinPage( pid, pg );
			if ( status != OK )
			{
				cerr << "*** Could not pin page " << pid << endl;
			}
			else if ( status == OK )
			{
				memcpy( &data, (void*)pg, sizeof data );
				data=data + increaser;
				memcpy( (void*)pg, &data, sizeof data );
				status = MINIBASE_BM->UnpinPage( pid , true );
				if ( status != OK )
					cerr << "*** Could not unpin page " << pid << endl;
			}
		}
	}

	cout << "  - Collect the Statistics\n";
	

	MINIBASE_BM->FlushAllPages();
    				     
	// record the end time
	endTime = clock();

    for (int index=0; index < numPages; index++ )
    {
		pid = pids[index];

        Status freeStatus = MINIBASE_BM->FreePage( pid );
        if ( status == OK && freeStatus != OK )
        {
            status = freeStatus;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }


	cout << "  - The running time for above operation is " << (endTime - initTime)*(1000.0/CLOCKS_PER_SEC) <<"ms\n";
	
	cout << "  - Starting to print Statistics \n";
	// Start to print statistics
	MINIBASE_BM->PrintStat();
	
    cout << "  Test 6 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();
    return true;
}

/**
 * This test case simulates the usage of pages in a particular workload in which 
 * an MRU replacement policy works better than an LRU replacement policy.
 */
int BMTester::Test7()
{
	// This test allocates a large number of pages and stores some value in them.
	// It then increments each page in numerical order. MRU performs better since 
	// some of the first allocated pages remain in memory when they are needed for
	// allocating. None of them are available under an LRU policy since when they 
	// are sufficiently old they are delted, and no page is available by the time
	// it is needed for incrementing. 

	Page* pg;
	PageID pid;
	Status status;
	int data;
	clock_t initTime, endTime;

	cout << "\n  Test 7 simulates an MRU-favorabe workload\n";

	// The number of pages will be allocated for this test
	const int numPages = 400; 

	// An array to save the pageID of the pages allocated
	PageID pids[numPages];

	//
	// Allocating Pages
	//
	
	// Record the initial running time 
	initTime = clock();

	// Start to collect statistics
	MINIBASE_BM->ResetStat();
	
	//
	// Write something to each page
	//
	for ( int index=0; index<numPages; index++ )
	{
		// Allocate page and pin it into buffer pool
		status = MINIBASE_BM->NewPage( pid, pg );
        if ( status != OK ){
            cerr << "*** Could not allocate new page number " << index+1 << endl;
			return FAIL;
		}
	
		pids[index] = pid;
		if ( status == OK )
		{
			//copy some value into the page
			data = pid + 99999;
			memcpy( (void*)pg, &data, sizeof data );


			status = MINIBASE_BM->UnpinPage( pid , true);
			if ( status != OK )
				cerr << "*** Could not unpin dirty page " << pid << endl;
		}
	}


	cout << "  - Go through Each allocated page in numerical order n times \n";
	cout << "       and increment it's value by increaser variable \n";
	 
	int increaser = 5;
	int n = 20;

	cout << "  - Collect the Statistics\n";
	
	for ( int i= 0; i<n; i++){
		for ( pid=pids[0]; status == OK && pid < pids[numPages-1]; ++pid )
		{  
			status = MINIBASE_BM->PinPage( pid, pg );
			if ( status != OK )
			{
				cerr << "*** Could not pin page " << pid << endl;
			}
			else if ( status == OK )
			{
				memcpy( &data, (void*)pg, sizeof data );
				data=data + increaser;
				memcpy( (void*)pg, &data, sizeof data );

				status = MINIBASE_BM->UnpinPage( pid , true );
				if ( status != OK )
					cerr << "*** Could not unpin page " << pid << endl;
			}
		}
	}

	MINIBASE_BM->FlushAllPages();
    				     
	// record the end time
	endTime = clock();

    for (int index=0; index < numPages; index++ )
    {
		pid = pids[index];

        Status freeStatus = MINIBASE_BM->FreePage( pid );
        if ( status == OK && freeStatus != OK )
        {
            status = freeStatus;
            cerr << "*** Error freeing page " << pid << endl;
        }
    }


	cout << "  - The running time for above operation is " << (endTime - initTime)*(1000.0/CLOCKS_PER_SEC) <<"ms\n";
	
	cout << "  - Starting to print Statistics \n";
	// Start to print statistics
	MINIBASE_BM->PrintStat();
	
    cout << "  Test 7 completed successfully.\n";

	cout << "  Press any key to continue." << endl;
	getch();
    return true;
}


const char* BMTester::TestName()
{
    return "Buffer Management";
}


void BMTester::RunTest( Status& status, TestDriver::testFunction test )
{
    minibase_globals = new SystemDefs( status, dbpath, logpath, 
				  NUMBUF+20, 500, NUMBUF, "Clock" );
    if ( status == OK )
    {
        TestDriver::RunTest(status,test);
        delete minibase_globals, minibase_globals = 0;
    }

    char* newdbpath;
    char* newlogpath;
    char remove_logcmd[50];
    char remove_dbcmd[50];

    newdbpath = new char[ strlen(dbpath) + 20];
    newlogpath = new char[ strlen(logpath) + 20];
    strcpy(newdbpath,dbpath);
	strcpy(newlogpath, logpath);

    sprintf(remove_logcmd, "del %s", logpath);
	sprintf(remove_dbcmd, "del -rf %s", dbpath);
	system(remove_logcmd);
	system(remove_dbcmd);
	sprintf(newdbpath, "%s", dbpath);
	sprintf(newlogpath, "%s", logpath);

    unlink( newdbpath );
    unlink( newlogpath );

    delete newdbpath; delete newlogpath;

}


Status BMTester::RunTests()
{
    return TestDriver::RunTests();
}


Status BMTester::RunAllTests()
{
    return TestDriver::RunAllTests();
}
