
#include "cat.h"


char cat_buffer[CAT_BUFFER_LEN];    // streaming is performed by reading and writing blocks of data

char cat_source_filename[64];       // the file name that is being read during the streaming process
char cat_dest_filename[64];         // the file name that is being written to during the streaming process

volatile bool cat_enabled = false;  // whether we are currently streaming or not
volatile bool cat_dry_run = false;  // whether we are saving the stream to a file

pthread_t cat_thread_id = 0;

void *cat_thread_entry(void *arg)
{
	FILE *dest = 0;

	// a dry run streams data from the kernel module, but does not save
	// the stream anywhere. Only open the destination file if this is not
	// a dry run
	if(!cat_dry_run)
	{
		// create the destination file and open a handle to it
		dest = fopen(cat_dest_filename, "wb");
		if(!dest)
		{
			printf("cat could not open destination file %s\n", cat_dest_filename);
			return;
		}

		// fill buffer with predictable data for debug purposes
		memset(cat_buffer, 0x5A, CAT_BUFFER_LEN);
	}

	// open the source file
	int source = open(cat_source_filename, O_RDONLY);
	if(!source)
	{
		fclose(dest);
		printf("cat could not open source file %s\n", cat_source_filename);
		return;
	}

	// debug info
	printf("cat started\n");

	// streaming is performed by reading and writing blocks of data sequentially
	while(cat_enabled)
	{
		// fill the buffer with data
		int bytes_read = read(source, cat_buffer, CAT_BUFFER_LEN);
		if(bytes_read <= 0)
		{
			printf("cat read error\n");
			break;
		}

		// if this is not a dry run, the data now needs to be saved to a file
		if(dest)
		{
			// write the same amount of bytes read, to the destination file.
			int bytes_written = fwrite(cat_buffer, 1, bytes_read, dest);
			if(bytes_written <= 0)
			{
				printf("cat write error\n");
				break;
			}
		}
	}

	// clean up
	close(source);
	if(dest)
		fclose(dest);
	printf("cat stopped\n");

	return NULL;
}



void cat_start(int file_counter)
{
	// cannot start repeatedly
	if(cat_enabled)
		return;

	// failsafe, disables writing if the file counter is not initialized
	if(file_counter == -1)
		cat_dry_run = true;

	// construct the file names
	sprintf(cat_source_filename, "/dev/pg_stream");
	sprintf(cat_dest_filename, "./records/measurement_%d.pgrecord", file_counter);

	// change status
	cat_enabled = true;

	// run the pump thread
	if(pthread_create(&(cat_thread_id), NULL, &cat_thread_entry, NULL))
	{
		printf("ERROR creating thread\n");
	}
}

void cat_stop()
{
	// set indicator LED to "stopping"
	pgdriver_send("gpio", "\x03\x01", 2); // output 1 on GPIO3
	pgdriver_send("gpio", "\x04\x00", 2); // output 0 on GPIO4

	// stop measurement
	cat_enabled = false;  // the pump thread monitors this variable and will quit ASAP
	cat_dry_run = false;

	// wait untill the pump thread stops, before proceeding. This ensures the streaming and
	// the measurement really stopped before this function returns
	if(cat_thread_id)
	{
		pthread_join(cat_thread_id, 0);
		cat_thread_id = 0;
	}

}


