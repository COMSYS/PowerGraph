#include "record_folder.h"


typedef void (*string_enumerate_callback)(char*);

// calls the callback function once for each file in the records folder,
// every time passing the name of the next file as parameter
void enumerate_records(string_enumerate_callback callback_function)
{
	DIR *d;
	struct dirent *dir;

	// open the folder as a file system handle
	d = opendir("records");
	if (d)
	{
		// get a pointer to a structure that contains information about the file
		while ((dir = readdir(d)) != NULL)
		{
			// call the callback function for each file, and pass the file name 
			callback_function(dir->d_name);
		}

		// clean up
		closedir(d);
	}
}

