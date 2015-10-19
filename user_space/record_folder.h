#ifndef _RECORD_FOLDER_
#define _RECORD_FOLDER_

#include "common.h"

#include <dirent.h> 


typedef void (*string_enumerate_callback)(char*);

// calls the callback function once for each file in the records folder,
// every time passing the name of the next file as parameter
void enumerate_records(string_enumerate_callback callback_function);

#endif