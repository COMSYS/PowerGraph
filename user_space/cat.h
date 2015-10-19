

#include "common.h"
#include "common.h"

//#define O_RDONLY 0

#define CAT_BUFFER_LEN 131072

extern volatile bool cat_enabled;
extern volatile bool cat_dry_run;


// Copy one file to another. Calling this function starts the streaming process.
void cat_start(int file_counter);


// Stops the streaming process
void cat_stop();


