#ifndef __DEVICE_REGISTRATION__
#define __DEVICE_REGISTRATION__

#include "common.h"

// =======================================================================================================
// ==============================  simple device file ====================================================
// =======================================================================================================


#define PRIVATE_DATA_OF_FILE(type_of_data, file_handle) (*(  (type_of_data *)file->private_data  ))

struct simple_device_file_handle
{
	struct cdev cdev;
	int major;
	struct class *device_class;
	const char *device_name;
};


// creates a linux device file, which will call the corresponding callback
// functions in the fops parameter when the device is being accessed in user space.
bool simple_device_file_register(struct simple_device_file_handle *hdev, const char* device_name, struct file_operations *fops);

// removes the device file from the system
void simple_device_file_unregister(struct simple_device_file_handle *hdev);


// =======================================================================================================
// ============================== simple message device file =============================================
// =======================================================================================================
// a device that allows a handler to construct a message once the device file is opened.
// can be written and copies the data into kernel space before calling the handler

struct messagefile_operations
{
	size_t (*build_message) (char **message_ptr);
	void (*on_message) (char *message_ptr, size_t message_size);
};

struct simple_device_messagefile_handle
{
	struct simple_device_file_handle simple_devh;
	struct messagefile_operations *mfops;

	// tracking file state, singleton
	bool device_already_open;  // prevent multiple opens
	uint8_t *msg_ptr;
	uint32_t bytes_left;
};

#define MESSAGEFILE_MAX_MESSAGE_SIZE 1024

// creates a device file that supports the "messagefile" interface
bool simple_device_messagefile_register(struct simple_device_messagefile_handle *mhdev, const char* device_name, struct messagefile_operations *mfops);

// removes the device file from the system
void simple_device_messagefile_unregister(struct simple_device_messagefile_handle *mhdev);



#endif