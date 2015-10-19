# Kernel Module

This is the kernel module for the PowerGraph suite.
You can compile the driver for the Raspberry pi using the supplied *Makefile*.
The driver creates the following files to communicate with the user space.

1. **/dev/pg_stream** - Opening and reading from this file instructs the driver to start the ADC hardware and to start streaming data on the file descriptor.

2. **/dev/pg_config** - This allows to read and write the calibration and configuration of PowerGraph. See **global_configuration.h**  for the struct that needs to be written/read. In addition, you can have a look at the user space tool and how it uses this file.

3. **/dev/pg_gpio** - This is a convenient way to set a GPIO pin on the Raspberry pi. Write two bytes to this file, the first byte identifies the GPIO pin and the second sets the mode:

+ 0x00 - set pin to output a logical 0
+ 0x01 - set pin to output a logical 1
+ 0x02 - set pin to input. In subsequent read of one byte will yield either a binary zero or one

## Files
+ main.c: contains the main code that initializes and manages DMA and all the resources, creates the device drivers, and streams data from bcm2835 chipset into the /dev/pg_stream file.
                         
+ common.h: common definitions and datatypes

+ bcm2835_interface.c: low level wrapper and interface for the of the bcm2835 chipset.

+ debug.c: helper functions for debug purposes

+ device_registration.c: linux device driver boilerplate

+ global_configuration.h: the config structure that is sent via the network is defined here

