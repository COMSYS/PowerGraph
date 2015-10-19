#include "storage.h"


void storage_save(char *save_name, void *data, int data_length)
{
	// save all of the data into the file and then close it

	FILE *f = fopen(save_name, "wb");
	if(f)
	{
		fwrite(data, data_length, 1, f);
		fclose(f);
	}
}


bool storage_load(char *save_name, void *data, int data_length)
{
	FILE *f = fopen(save_name, "rb");
	int success = false;
	if(f)
	{
		fseek(f, 0, SEEK_END);
		int file_size = ftell(f);
		fseek(f, 0, SEEK_SET);

		// if the file size is equal to structure size, assume that the
		// file actually contains a saved structure and load it
		if(file_size == data_length)
		{
			success = fread(data, data_length, 1, f);
			fclose(f);
		}
	}
	return success;
}




