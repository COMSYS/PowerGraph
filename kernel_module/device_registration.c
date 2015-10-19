#include "device_registration.h"


// template

//static int lcd_device_open(struct inode *, struct file *);
//static int lcd_device_release(struct inode *, struct file *);
//static ssize_t lcd_device_read(struct file *, char *, size_t, loff_t *);
//static ssize_t lcd_device_write(struct file *, const char *, size_t, loff_t *);*/
//static struct file_operations lcd_fops = {
//	.read = lcd_device_read,
//	.write = lcd_device_write,
//	.open = lcd_device_open,
//	.release = lcd_device_release
//};


// =======================================================================================================
// ==============================  simple device file ====================================================
// =======================================================================================================


#define PRIVATE_DATA_OF_FILE(type_of_data, file_handle) (*(  (type_of_data *)file->private_data  ))

bool simple_device_file_register(struct simple_device_file_handle *hdev, const char* device_name, struct file_operations *fops)
{
	// default linux boilerplate code to create a device file
	hdev->device_name = device_name;
	if (alloc_chrdev_region(&hdev->major, 0, 1, device_name) < 0)
	{
		printk(KERN_ALERT "alloc_chrdev_region failed\n");
		return false;
	}
	if((hdev->device_class = class_create(THIS_MODULE, hdev->device_name)) == NULL)
	{
		unregister_chrdev_region(hdev->major, 1);
		printk(KERN_ALERT "class_create failed\n");
		return false;
	}
	// this device_create replaces the user space "mknod /dev/<devname> c <major> 0" command
	if (device_create(hdev->device_class, NULL, hdev->major, NULL, hdev->device_name) == NULL)
	{
		class_destroy(hdev->device_class);
		unregister_chrdev_region(hdev->major, 1);
		printk(KERN_ALERT "device_create failed\n");
		return false;
	}
	cdev_init(&hdev->cdev, fops);
	hdev->cdev.owner = THIS_MODULE;
	if (cdev_add(&hdev->cdev, hdev->major, 1) == -1)
	{
		device_destroy(hdev->device_class, hdev->major);
		class_destroy(hdev->device_class);
		printk(KERN_ALERT "cdev_add failed\n");
		return false;
	}

	return true;
}

void simple_device_file_unregister(struct simple_device_file_handle *hdev)
{
	cdev_del(&hdev->cdev);
	device_destroy(hdev->device_class, hdev->major);
	class_destroy(hdev->device_class);
	unregister_chrdev(hdev->major, hdev->device_name);
}


// =======================================================================================================
// ============================== simple message device file =============================================
// =======================================================================================================
// a device that allows a handler to construct a message once the device file is opened.
// can be written and copies the data into kernel space before calling the handler

#define PRIVATE_DATA_OF_MESSAGEFILE(file_handle) (*(  (struct messagefile_operations*)file->private_data  ))

// "struct cdev" is the first one in "struct simple_device_file_handle". so just cast it
#define MESSAGEFILE_DEVICE_HANDLE_FROM_INODE(file_inode) (*(struct simple_device_messagefile_handle*)(  (struct simple_device_file_handle*)file_inode->i_cdev  ))

// Called when a process tries to open the device file, like "cat /dev/..."
static int _simple_device_messagefile_open(struct inode *inode, struct file *file)
{
	struct simple_device_messagefile_handle *mhdev = &MESSAGEFILE_DEVICE_HANDLE_FROM_INODE(inode);
	mhdev->device_already_open = true;

	mhdev->msg_ptr = "not implemented\n";
	mhdev->bytes_left = 16;

	if(mhdev->mfops->build_message)
	{
		mhdev->bytes_left = mhdev->mfops->build_message((char**)(&mhdev->msg_ptr));
	}

	// prevent the module from being unloaded
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

// Called when a process closes the device file.
static int _simple_device_messagefile_release(struct inode *inode, struct file *file)
{
	struct simple_device_messagefile_handle *mhdev = &MESSAGEFILE_DEVICE_HANDLE_FROM_INODE(inode);
	mhdev->device_already_open = false;

	// Decrement the usage count, or else once you opened the file, you'll never get get rid of the module. 
	module_put(THIS_MODULE);

	return SUCCESS;
}

// Called when a process, which already opened the dev file, attempts to read from it.
static ssize_t _simple_device_messagefile_read(struct file *file_handle,	// see include/linux/fs.h
			   char *buffer,						        // buffer to fill with data
			   size_t requested_byte_count,						        // length of the buffer
			   loff_t * offset)
{
	struct simple_device_messagefile_handle *mhdev = &MESSAGEFILE_DEVICE_HANDLE_FROM_INODE(file_handle->f_dentry->d_inode);

	// Number of bytes actually written to the buffer 
	int bytes_read = 0;

	while(mhdev->bytes_left > 0 && requested_byte_count > 0)
	{
		// The buffer is in the user data segment, not the kernel 
		// segment so "*" assignment won't work.  We have to use 
		// put_user which copies data from the kernel data segment to
		// the user data segment. 
		put_user(*mhdev->msg_ptr, buffer++);

		bytes_read++;
		mhdev->msg_ptr++;
		mhdev->bytes_left--;
		requested_byte_count--;
	}
	return bytes_read;
}

#define MESSAGEFILE_MAX_MESSAGE_SIZE 1024
char _simple_device_messagefile_message_buffer[MESSAGEFILE_MAX_MESSAGE_SIZE];

// Called when a process writes to dev file: echo "hi" > /dev/hello 
// memo: could check if user space pointer is valid with access_ok()
static ssize_t _simple_device_messagefile_write(struct file *file_handle, const char *user_buffer, size_t bytes_available, loff_t * offset)
{
	int bytes_left = bytes_available;

	// ignore messages that are too big, misuse of this driver
	if(bytes_left > MESSAGEFILE_MAX_MESSAGE_SIZE)
		return bytes_left;  // signal that all bytes were handled

	// ignore message if no message handler is registered
	struct simple_device_messagefile_handle *mhdev = &MESSAGEFILE_DEVICE_HANDLE_FROM_INODE(file_handle->f_dentry->d_inode);
	if(!mhdev->mfops->on_message)
		return bytes_left;  // signal that all bytes were handled

	// copy message from user memory to 
	char *p = _simple_device_messagefile_message_buffer;
	while(bytes_left > 0)
	{
		get_user(*p, user_buffer);
		bytes_left--;
		user_buffer++;
		p++;
	}

	// call message handler
	mhdev->mfops->on_message(_simple_device_messagefile_message_buffer, bytes_available);

	// signal success, all bytes were handled
	return bytes_available;
}

static struct file_operations _simple_device_messagefile_fops = {
	.read    = _simple_device_messagefile_read,
	.write   = _simple_device_messagefile_write,
	.open    = _simple_device_messagefile_open,
	.release = _simple_device_messagefile_release
};


bool simple_device_messagefile_register(struct simple_device_messagefile_handle *mhdev, const char* device_name, struct messagefile_operations *mfops)
{
	// based upon the simple_device interface, the linux device file boilerplate code is called from there
	if(!simple_device_file_register(&mhdev->simple_devh, device_name, &_simple_device_messagefile_fops))
		return false;
	mhdev->device_already_open = false;
	mhdev->mfops = mfops;
	return true;
}

void simple_device_messagefile_unregister(struct simple_device_messagefile_handle *mhdev)
{
	simple_device_file_unregister(&mhdev->simple_devh);
}
