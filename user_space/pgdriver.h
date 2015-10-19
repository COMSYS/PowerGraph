#ifndef _PGDRIVER_
#define _PGDRIVER_

#include "common.h"

// Sends a message to the pgdriver kernel module by writing the message to the
// respective file in /dev/. The channel parameter must be one of these string:
// "stream" - writes to /dev/pg_stream
// "config" - writes to /dev/pg_config
// "gpio" - writes to /dev/pg_gpio
void pgdriver_send(char *channel, void* buffer, int buffer_length);

// Reads at most 4 bytes from the kernel module. The channel must be one of the
// strings just like in pgdriver_send.
// returns the result in the lower bytes of the returned integer.
int pgdriver_get_small(char *channel, int small_size);


#endif