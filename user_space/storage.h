#ifndef _STORAGE_
#define _STORAGE_


#include "common.h"

// saves the given structure to a file
#define storage_save_structure(save_name, structure) storage_save(save_name, &structure, sizeof(structure))
void storage_save(char *save_name, void *data, int data_length);


// loads a file into a given structure
#define storage_load_structure(save_name, structure) storage_load(save_name, &structure, sizeof(structure))
bool storage_load(char *save_name, void *data, int data_length);


#endif