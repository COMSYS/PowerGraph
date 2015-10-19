#ifndef _COMMON_
#define _COMMON_


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/param.h>

#include <pthread.h>
#include <ifaddrs.h>



typedef int bool;
#define true (1)
#define false (0)



// a fatal error function. defined separatelly to easily adjust error behavior
void error(char *msg);

// waits a given millisecond amount, before returning
void msleep(int msecs);

#endif