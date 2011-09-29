#include <stdlib.h>
#include <iostream>
#include <conio.h>

using namespace std;

#include "bmtest.h"

int MINIBASE_RESTART_FLAG = 0;

int main (int argc, char **argv)
{
	BMTester tester;
	Status status;

	int bufSize = NUMBUF;

	const char* replacement_policies[] = {"LRU", "MRU"};
	int numPolicies = 2;

	for (int i = 0; i < numPolicies; i++) {
		cout << "Running the Buffer Manager with " 
			 << replacement_policies[i] << " replacement policy." << endl;

		minibase_globals = new SystemDefs(status, "MINIBASE.DB", 2000, bufSize, replacement_policies[i]);

		if (status != OK)
		{
			cerr << "Error initializing Minibase.\n";
			exit(2);
		}

		status = tester.RunTests();

		if (status != OK)
		{
			cout << "Error running buffer manager tests\n";
			minibase_errors.show_errors();
			return 1;
		}

		delete minibase_globals;
		cout << endl;
	}
	getch();
	return 0;
}