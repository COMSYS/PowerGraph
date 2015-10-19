#include "common.h"

void error(char *msg)
{
	perror("error: ");
	perror(msg);
	exit(1);
}

void msleep(int msecs)
{
	usleep(msecs * 1000);
}
