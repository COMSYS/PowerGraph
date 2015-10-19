#include "pgdriver.h"


char pg_device_file_name[32];


// sends a message to the kernel module
void pgdriver_send(char *channel, void* buffer, int buffer_length)
{
	// construct the device file name that corresponds to the kernel module communication channel
	sprintf(pg_device_file_name, "/dev/pg_%s", channel);

	// open the device file
	FILE *f = fopen(pg_device_file_name, "wb");
	if(f)
	{
		// write the message to the device file, which immediatelly calls the corresponding function
		// in kernel space
		fwrite(buffer, 1, buffer_length, f);
		fclose(f);
	}
	else
	{
		printf("could not open device file %s\n", pg_device_file_name);
	}
}


// receives up to 4 bytes from the kernel space module
int pgdriver_get_small(char *channel, int small_size)
{
	if(small_size > 4)
		exit(1); // this should never happen

	int retval = 0;

	// construct the device file name that corresponds to the kernel module communication channel
	sprintf(pg_device_file_name, "/dev/pg_%s", channel);

	// open the device file
	FILE *f = fopen(pg_device_file_name, "rb");
	if(f)
	{
		// reading from the device file immediatelly calls the corresponding kernel space
		// function, which constructs a response and only then will fread return.
		fread(&retval, 1, small_size, f);
		fclose(f);
	}
	else
	{
		printf("could not open device file %s\n", pg_device_file_name);
	}
	return retval;
}
