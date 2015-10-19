#include "common.h"
#include "global_configuration.h"
#include "device_registration.h"
#include "debug.h"
#include "bcm2835_interface.h"



#define CORE_FREQ (250000000)

// the default config variables, which will be overwritten with more relevant values when possible.
struct global_config_struct global_config = {"unnamed device", "A", 1, 0, 1, 0, 0, 0x3FFF, 0x0FFF0000, 0, 0, 0, 122070, 2, 0, 0, 0};




// =======================================================================================================
// ==============================  DMA  ==================================================================
// =======================================================================================================

// ------------------------------ data -------------------------------------------------------------------

struct dma_t spi0_dma_in;
struct dma_t spi0_dma_out;

struct dma_mempage_t spidma_mempage_out_buffer;    // data that will be pushed into the ADC
struct dma_mempage_t spidma_mempage_in_buffer[2];  // the ADC data stream will be filled in here

uint32_t *spidma_out_buffer;
uint32_t *spidma_in_buffer[2];


// ------------------------------ code -------------------------------------------------------------------

bool spidma_init(void)
{
	dma_reserve_channel(&spi0_dma_in);
	dma_reserve_channel(&spi0_dma_out);

	if(!dma_alloc_page(&spidma_mempage_out_buffer, 1))
		return false;
	if(!dma_alloc_page(&spidma_mempage_in_buffer[0], 8))
		return false;
	if(!dma_alloc_page(&spidma_mempage_in_buffer[1], 8))
		return false;
	
	// use page directly as structure
	spidma_out_buffer   = (uint32_t*) spidma_mempage_out_buffer  .virtual_ptr;
	spidma_in_buffer[0] = (uint32_t*) spidma_mempage_in_buffer[0].virtual_ptr;
	spidma_in_buffer[1] = (uint32_t*) spidma_mempage_in_buffer[1].virtual_ptr;
	
	// mark the buffer for debugging purposes
	{
		uint32_t* p1 = spidma_in_buffer[0];
		uint32_t* p2 = spidma_in_buffer[1];
		int size = spidma_mempage_in_buffer[0].size / sizeof(uint32_t);
		int i;
		for(i=0; i < size; i++)
		{
			p1[i] = 1;
			p2[i] = 2;
		}
	}

	// setup the DMA control blocks so that
	// - the block with index 0 is reading from SPI and filling spidma_mempage_in_buffer[0]
	// - the block with index 1 is reading from SPI and filling spidma_mempage_in_buffer[1]
	// - the block with index 2 is writing to SPI the contents of spidma_mempage_out_buffer

	dma_setup_control_block(0,                 // cb_index
		spidma_mempage_in_buffer[0].size,      // transfer_byte_count
		SPI0_FIFO_BUS_ADDRESS,                 // source_bus_address
		DMA_DO_NOT_INCREMENT,                  // source_increment
		spidma_mempage_in_buffer[0].bus_ptr,   // destination_bus_address
		DMA_INCREMENT,                         // destination_increment
		DREQ_SPI0_RX,                          // dreq_source
		true,                                  // dreq_gates_reads
		false,                                 // dreq_gates_writes
		false,                                 // wait_for_write_confirmation
		1,                                     // add_wait_cycles_per_readwrite
		true,                                  // generate_interrupt
		1);                                    // next_cb_index

	dma_setup_control_block(1,                 // cb_index
		spidma_mempage_in_buffer[1].size,      // transfer_byte_count
		SPI0_FIFO_BUS_ADDRESS,                 // source_bus_address
		DMA_DO_NOT_INCREMENT,                  // source_increment
		spidma_mempage_in_buffer[1].bus_ptr,   // destination_bus_address
		DMA_INCREMENT,                         // destination_increment
		DREQ_SPI0_RX,                          // dreq_source
		true,                                  // dreq_gates_reads
		false,                                 // dreq_gates_writes
		false,                                 // wait_for_write_confirmation
		1,                                     // add_wait_cycles_per_readwrite
		true,                                  // generate_interrupt
		0);                                    // next_cb_index

	dma_setup_control_block(2,                 // cb_index
		32,                                    // transfer_byte_count
		spidma_mempage_out_buffer.bus_ptr,     // source_bus_address
		DMA_INCREMENT,                         // source_increment
		SPI0_FIFO_BUS_ADDRESS,                 // destination_bus_address
		DMA_DO_NOT_INCREMENT,                  // destination_increment
		DREQ_SPI0_TX,                          // dreq_source
		false,                                 // dreq_gates_reads
		true,                                  // dreq_gates_writes
		true,                                  // wait_for_write_confirmation
		4,                                     // add_wait_cycles_per_readwrite
		false,                                 // generate_interrupt
		2);                                    // next_cb_index

	return true;
}

void spidma_start(void)
{
	// fill output buffer
	{
		uint8_t* p = (uint8_t*)spidma_out_buffer;
		int i;
		int m;
		uint32_t pattern = global_config.SPI_output; // good test pattern is: 0FFF0000
		for(i = 0; i < 128; i++)
		{
			m = i % 4;
			/*
			if(m == 0)
				p[i] = 0x0F;
			else
			if(m == 1)
				p[i] = 0xFF;
			else
			if(m == 2)
				p[i] = 0x00;
			else
			if(m == 3)
				p[i] = 0x00;
			*/
			p[i] = (pattern >> (8*(3 - m))) & 0xFF;
		}
	}

	// start DMA

	dma_start(&spi0_dma_in,  // dma channel
		0,                   // first_control_block_index
		8,                   // normal_priority
		15);                 // panic_priority
	dma_start(&spi0_dma_out, // dma channel
		2,                   // first_control_block_index
		8,                   // normal_priority
		15);                 // panic_priority
}

void spidma_stop_and_reset(void)
{
	dma_stop_and_reset(&spi0_dma_in);
	dma_stop_and_reset(&spi0_dma_out);
}

void spidma_cleanup(void)
{
	dma_free_page(&spidma_mempage_out_buffer);
	dma_free_page(&spidma_mempage_in_buffer[0]);
	dma_free_page(&spidma_mempage_in_buffer[1]);
	dma_free_channel(&spi0_dma_out);
	dma_free_channel(&spi0_dma_in);
}


// =======================================================================================================
// ==============================  stream device  ========================================================
// =======================================================================================================

struct simple_device_file_handle   stream_device;
static struct file_operations      stream_device_fops;
bool                               stream_device_is_open;

char *debug_error_message = "no error";

static   int stream_device_next_dma_buffer_index;
volatile int stream_device_overload_times;
volatile int stream_device_overload_times_total;
static   int min_stream_request_length = 99999999;    // keep track of the request size for debug purposes

static   uint32_t stream_device_raw_sample = 0;  // save a raw sample from each batch for debug purposes

static   bool stream_device_is_beginning = true;

static   bool stream_device_debug_mode = false;

static   long stream_device_accumulator_sum = 0;
static   int stream_device_accumulator_count = 0;


// Called when a process tries to open the device file, like "cat /dev/pg_stream"
static int stream_device_open(struct inode *inode, struct file *file)
{
	// do not allow to open the device twice
	if (stream_device_is_open)
		return -EBUSY;
	stream_device_is_open = true;

	// init status variables
	stream_device_overload_times_total = 0;
	stream_device_overload_times = 0;
	stream_device_next_dma_buffer_index = 0;
	stream_device_is_beginning = true;

	// prevent the module from being unloaded
	try_module_get(THIS_MODULE);

	// switch the physical GPIO pins, connect them to the internal spi0 peripheral
	spi0_setup_gpio_pins(true, true, true, true);
//	spi0_set_divider(0x10); // 15 Mhz
//	spi0_set_divider(0x80); // 2 Mhz

	// set the spi0 clock frequency
	spi0_set_divider(CORE_FREQ / global_config.sample_rate / 32);

	spi0_set_remaining_transfer_byte_count(0xFFFF);
	spi0_set_dma_signal_threshholds( /*r_req*/ 32, /*r_panic*/ 48, /*t_req*/ 32, /*t_panic*/ 24);
	spi0_set_clock_mode(true, false);
	spi0_set_dma_mode(true, false);
	spi0_clear_buffers(true, true);
	spi0_start();

	// start DMA
	spidma_start();

	// reset compounder
	stream_device_accumulator_sum = 0;
	stream_device_accumulator_count = 0;

	// turn on indicator LED
	gpio_pin_set_output(3, false);
	gpio_pin_set_output(4, true);

	return SUCCESS;
}


// Called when a process closes the device file.
static int stream_device_release(struct inode *inode, struct file *file)
{
	stream_device_is_open = false;	// We're now ready for our next caller

	// Decrement the usage count, or else once you opened the file, you'll never get get rid of the module. 
	module_put(THIS_MODULE);

	// stop the streaming process
	spi0_stop();
	spidma_stop_and_reset();

	// turn off indicator LED
	gpio_pin_set_output(3, false);
	gpio_pin_set_output(4, false);

	return 0;
}


// reverses the byte order on the integer
int inline reverse_bytes_int(int a)
{
	return ((a&0xFF) << 24) | ((a&0xFF00) << 8) | ((a&0xFF0000) >> 8) | (a >> 24);
}


// sets all bits to 1 that are lower from existing 1-bits.  e.g.   0x20 is converted into 0x3F
short make_mask(short a)
{
	a |= a >> 8;
	a |= a >> 4;
	a |= a >> 2;
	a |= a >> 1;
	return a;
}

// Called when a process, which already opened the dev file, attempts to read from it.
static ssize_t stream_device_read(struct file *file_handle,	// see include/linux/fs.h
			   char *req_buffer,						        // buffer to fill with data
			   size_t req_length,						        // length of the buffer
			   loff_t * offset)
{
	char *initial_req_buffer = req_buffer;
	// Number of bytes actually written to the buffer 
	int bytes_read = 0;

	// write down the request length to see how small the read buffers are
	if(req_length < min_stream_request_length)
		min_stream_request_length = req_length;

	// if window too small, don't even start
	if(req_length < spidma_mempage_in_buffer[0].size)
	{
		debug_error_message = "window size smaller than buffersize. which is too small.";
		return 0;
	}

	// deadline time when we give up waiting for DMA to finish, because then it's probably stuck anyway
	uint32_t deadline = current_system_msec_time() + 2000;

	while(true)
	{
		// get currently filled DMA buffer
		int current_DMA_buffer = dma_get_channel_current_control_block_index(&spi0_dma_in);
		if(current_DMA_buffer == DMA_INDEX_NO_MORE || current_DMA_buffer == DMA_INDEX_ERROR)
		{
			debug_error_message = "DMA stopped running by itself. this is unexpected behaviour, DMA should run on an infinite loop and never stops";
			return 0;
		}
		// if the currently by DMA filled buffer is not the one we need to read, then there is no conflict
		if(current_DMA_buffer != stream_device_next_dma_buffer_index)
		{
			// DMA is finally filling the other buffer. we can read this one, there is no conflict
			break;
		}

		// ensure the transfer never stops by resetting this value to highest possible
		spi0_set_remaining_transfer_byte_count(0xFFFF);

		// release control of CPU
		msleep(0);

		// quit when deadline is reached
		if(current_system_msec_time() > deadline)
		{
			debug_error_message = "waited too long for DMA to finish the current Control Block. DMA is stuck, probably because SPI0 stopped working, maybe because the LEN register of SPI0 ran down to zero";
			return 0;
		}
	}

	// reset overload counter. currently overloads are not mentioned in stream
	stream_device_overload_times = 0;

	// pointer to the DMA source buffer
	uint32_t *source_ptr = spidma_in_buffer[stream_device_next_dma_buffer_index];

	// save a raw sample for debug purposes
	stream_device_raw_sample = reverse_bytes_int(*source_ptr);

	// Actually put the data into the buffer 
	if(!stream_device_debug_mode)
	{
		// The buffer is in the user data segment, not the kernel 
		// segment so "*" assignment won't work.  We have to use 
		// put_user which copies data from the kernel data segment to
		// the user data segment. copy_to_user
#define put_item(item_type, value) put_user(value, (item_type*)req_buffer);req_buffer += sizeof(item_type);

		if(stream_device_is_beginning) {
			stream_device_is_beginning = false;

			// write magic string
			int j = 0;
			char *s = "power graph record";
			for(j = 0; s[j]; j++) {
				put_item(char, s[j]);
			}

			// write the unit name
			for(j = 0; j < 16; j++) {
				put_item(char, global_config.unit_name[j]);
			}

			// write unit range
			put_item(short, global_config.unit_significand_a);
			put_item(char,  global_config.unit_exponent_a);

			// sample rate
			put_item(int, global_config.sample_rate / global_config.downsampling_rate);

			// analog signal count
			put_item(char, 1);

			// digital signal count
			put_item(char, 1);

			// ADC range
			put_item(short, global_config.ADC_max_value - global_config.ADC_min_value);

			// reserved bytes
			for(j = 0; j < 7; j++) {
				put_item(char, 0);
			}
		}

		// precalculate important values to maximise loop performance
		const int bit_shift = global_config.bit_shift;
		const short mask = make_mask(global_config.ADC_max_value);
		const uint32_t digital_channel_mask = 1 << global_config.digital_bit_shift;
		const short digital_output_mask = 0x0001;
		const short min_value = global_config.ADC_min_value;

		long accumulator_sum = stream_device_accumulator_sum;
		int accumulator_count = stream_device_accumulator_count;
		const int accumulator_max = global_config.downsampling_rate;

		int i;
		int sample_count = spidma_mempage_in_buffer[0].size / sizeof(uint32_t);
		for(i=0; i < sample_count; i++)
		{
			// SPI delivers sample in reversed byte order
			const uint32_t raw = reverse_bytes_int(*(source_ptr++));

			short value = ((raw >> bit_shift) & mask);
			if(value < min_value)
				value = 0;
			else
				value -= min_value;

			accumulator_sum += value;

			if(++accumulator_count == accumulator_max)
			{
				put_item(short, ((accumulator_sum / accumulator_max) << 1) | ((raw & digital_channel_mask)?(digital_output_mask):(0)) );

				accumulator_count = 0;
				accumulator_sum = 0;
			}

		}

		stream_device_accumulator_sum = accumulator_sum;
		stream_device_accumulator_count = accumulator_count;
	}
	// put a hexadecimal representation of the first sample
	else
	{
		char tmp[16];
		sprintf(tmp, "%08x\r", stream_device_raw_sample);
		int i;
		for(i = 0; i < 9; i++)
			put_user(tmp[i], req_buffer++);
		bytes_read = i;
	}
	
	// calculate the next buffer index that will be handled
	stream_device_next_dma_buffer_index = 1-stream_device_next_dma_buffer_index; //if it was 0, make it 1 and vice versa

	// Most read functions return the number of bytes put into the buffer
	// end of file is signalled by bytes_read==0
	return req_buffer - initial_req_buffer;
}


// Called when a process writes to dev file: echo "hi" > /dev/hello 
static ssize_t stream_device_write(struct file *file_handle, const char *user_buffer, size_t len, loff_t * offset)
{
	// ignore. but signal that all bytes have been successfully written
	return len;
}


static struct file_operations stream_device_fops = {
	.read    = stream_device_read,
	.write   = stream_device_write,
	.open    = stream_device_open,
	.release = stream_device_release
};


// =======================================================================================================
// ==================================  debug device  =====================================================
// =======================================================================================================
// the debug device can be read from user space and returns the last error message generated
// inside the kernel module.

struct simple_device_messagefile_handle   debug_device;
static struct messagefile_operations      debug_device_fops;

char debug_device_message[1024];

size_t debug_device_build_message(char **message_ptr)
{
	char *message;

	message = debug_device_message;
	sprintf(message, "err: %s\n time: %u\n minimum req_length: %u\n total overloads: %u\n current DMA cb: %d\n",
		debug_error_message,
		current_system_msec_time(),
		min_stream_request_length,
		stream_device_overload_times_total,
		dma_get_channel_current_control_block_index(&spi0_dma_in));

	message += strlen(message);
	// show which DMA channels are used and when
#define DMAA(a) (io_dma->DMA[a].IO_BLOCK.DEST_AD)
	sprintf(message, "dma: 0-%x 1-%x 2-%x 3-%x 4-%x 5-%x 6-%x 7-%x 8-%x 9-%x A-%x B-%x C-%x D-%x E-%x\n",
		DMAA(0), DMAA(1), DMAA(2), DMAA(3), DMAA(4), DMAA(5), DMAA(6), DMAA(7), DMAA(8), DMAA(9), DMAA(10), DMAA(11), DMAA(12), DMAA(13), DMAA(14));
#undef DMAA
	message += strlen(message);

	// show spi0 register content
	sprintf(message, "dma_buffer: CS: %x  FIFO: %x  CLK: %x  DLEN: %x  LTOH: %x  DC: %x\n",
		io_spi0->CS, io_spi0->FIFO, io_spi0->CLK, io_spi0->DLEN, io_spi0->LTOH, io_spi0->DC);
	message += strlen(message);

	sprintf(message, "config freq: %d SPI_out: %08X unit significand: %d digital channel shift: %d\n", global_config.sample_rate, global_config.SPI_output, global_config.unit_significand_a, global_config.digital_bit_shift);

	*message_ptr = debug_device_message;
	return strlen(debug_device_message);
}

void debug_device_on_message(char *message_ptr, size_t message_size)
{
	if(message_size > 0)
		stream_device_debug_mode = message_ptr[0] & 1;
}


static struct messagefile_operations debug_device_fops = {
	.build_message = debug_device_build_message,
	.on_message    = debug_device_on_message,
};


// =======================================================================================================
// ==================================  config device  ====================================================
// =======================================================================================================

struct simple_device_messagefile_handle   config_device;
static struct messagefile_operations      config_device_fops;

size_t config_device_build_message(char **message_ptr)
{
	*message_ptr = (char*)&stream_device_raw_sample;
	return 4;
}

void config_device_on_message(char *message_ptr, size_t message_size)
{
	// configuration is set by writing the config structure into this device
	if(message_size == sizeof(struct global_config_struct))
	{
		// copy all config data into our kernel config structure
		global_config = *((struct global_config_struct*)message_ptr);
	}
	else
	{
		debug_error_message = "config file was written something other than a valid config";
	}
}

static struct messagefile_operations config_device_fops = {
	.build_message = config_device_build_message,
	.on_message    = config_device_on_message,
};


// =======================================================================================================
// ==================================  gpio   device  ====================================================
// =======================================================================================================

struct simple_device_messagefile_handle   gpio_device;
static struct messagefile_operations      gpio_device_fops;

char gpio_read_pin_number = 1;
char gpio_read_message;

void gpio_device_on_message(char *message_ptr, size_t message_size)
{
	if(message_size == 2)
	{
		// save the pin number, so that reading from this device would yield the state of this same GPIO pin
		gpio_read_pin_number = message_ptr[0];
		gpio_pin_select_function(gpio_read_pin_number, (message_ptr[1] == 2)?(GPIO_PIN_MODE_INPUT):(GPIO_PIN_MODE_OUTPUT));
		if(message_ptr[1] < 2)
			gpio_pin_set_output(gpio_read_pin_number, message_ptr[1] & 1);
	}
	else
	{
		debug_error_message = "gpio file was written something other than a valid 2 byte gpio command";
	}
}

size_t gpio_device_build_message(char **message_ptr)
{
	// read from this device file reads the GPIO pin that was accesses most recently by writing
	gpio_read_message = (char)!!gpio_pin_get_input(gpio_read_pin_number);
	*message_ptr = &gpio_read_message;

	// only 1 byte can be read from this file
	return 1;
}

static struct messagefile_operations gpio_device_fops = {
	.build_message = gpio_device_build_message,
	.on_message    = gpio_device_on_message,
};


// =======================================================================================================
// ================================  blink_LED  ==========================================================
// =======================================================================================================


// shortly blinks the status LED
void blink_LED(void)
{
	gpio_pin_set_output(4, true);
	msleep(300);
	gpio_pin_set_output(4, false);
	msleep(300);
}

// toggles the status LED
void toggle_LED(void)
{
	static bool on = false;
	on = !on;
	gpio_pin_set_output(4, on);
}


// =======================================================================================================
// ================================  entry point  ========================================================
// =======================================================================================================


uint8_t runner = 0;

void dma_callback(void)
{
	// clear the interrupt request.
	dma_clear_channel_flags(&spi0_dma_in);

	// ensure the transfer never stops by resetting this value to highest possible
	spi0_set_remaining_transfer_byte_count(0xFFFF);

	// keep track of overloads. This needs to be done in the interrupt handler, because outside of the
	// interrupt handler of the DMA there is no reliable way to determine how many times the DMA switched
	// the next CB (Control Block).
	if(stream_device_is_open)
	{
		// if the next DMA buffer to be filled is the same as the one being encoded
		if(dma_get_channel_current_control_block_index(&spi0_dma_in) == stream_device_next_dma_buffer_index)
		{
			stream_device_overload_times++;
			stream_device_overload_times_total++;
		}
	}

	// for debug purposes, the blinking rate of the LED corresponds to the
	// dma switching rate and buffer fill rate
	toggle_LED();
}

int init_module(void)
{
	printk(KERN_INFO "PGBoard driver entrypoint.\n");

	// map hardware registers to memory
	if(!bcm2835_init())
	{
		printk(KERN_ERR "bcm2835_init() failed\n");
		return -EIO;
	}


	// setup gpio
	{
		gpio_reset();

		// turn off measurement LED
		gpio_pin_select_function(3, GPIO_PIN_MODE_OUTPUT);
		gpio_pin_select_function(4, GPIO_PIN_MODE_OUTPUT);
		gpio_pin_set_output(3, false);
		gpio_pin_set_output(4, false);

		// turn off booting LED
		gpio_pin_select_function(2, GPIO_PIN_MODE_OUTPUT);
		gpio_pin_set_output(2, false);
	}

	// reset SPI0
	spi0_stop();
	spi0_reset();

	// setup DMA
	if(!spidma_init())
	{
		printk(KERN_ERR "spi_dma_init() failed\n");
		return -EIO;
	}
	spidma_stop_and_reset();
	dma_interrupt_bind(dma_callback, 10);

	// make linux filesystem device files
	if(!simple_device_file_register(&stream_device, "pg_stream", &stream_device_fops))
		return -EIO;

	if(!simple_device_messagefile_register(&debug_device, "pg_debug", &debug_device_fops))
		return -EIO;

	if(!simple_device_messagefile_register(&config_device, "pg_config", &config_device_fops))
		return -EIO;

	if(!simple_device_messagefile_register(&gpio_device, "pg_gpio", &gpio_device_fops))
		return -EIO;


	// all done
	printk(KERN_INFO "PGBoard driver online.\n");
	return SUCCESS;
}

void cleanup_module(void)
{
	// reset DMAs
	dma_interrupt_free(10);
	spidma_stop_and_reset();
	udelay(100);
	spi0_stop();
	gpio_reset();
	simple_device_file_unregister(&stream_device);
	simple_device_messagefile_unregister(&debug_device);
	simple_device_messagefile_unregister(&config_device);
	simple_device_messagefile_unregister(&gpio_device);

	// DMA buffers
	spidma_cleanup();

	printk(KERN_INFO "PGBoard driver offline.\n");
}


// =======================================================================================================
// ==================================  meta data  ========================================================
// =======================================================================================================

MODULE_AUTHOR("Roman Bartaschwitsch, Jan Rüth, Torsten Zimmermann");
MODULE_DESCRIPTION("PowerGraph Driver and Service Module");	/* What does this module do */
MODULE_SUPPORTED_DEVICE("pgboard");

