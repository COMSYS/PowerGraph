#include "common.h"
#include "network.h"
#include "pgdriver.h"
#include "record_folder.h"
#include "storage.h"
#include "cat.h"


// this structure contains all the variables available for configuration of PowerGraph. It is sent,
// as is, over the network when configuring PowerGraph through the GUI, and this structure is also stored
// in a file, as is, to remember the configuration between Rapberry Pi restarts.
struct global_config_struct
{
	// general config
	char device_name[32];   // usefull for indentification if you have more than one PowerGraph tool
	char unit_name[16];     // name of the physical measurement unit
	int unit_significand_a; // how much of the physical unit is being measured when ADC_max_value is reached
	int unit_exponent_a;    // base 10 (to simplify visualization across any plattform)
	int unit_significand_b; // reserved for later, if measurement range switching is to be implemented
	int unit_exponent_b;    // base 10
	int ADC_min_value;      // this ADC value corresponds to a physical measurement of zero units
	int ADC_max_value;      // determines bit count, for 14bits this value must be between 0x2000 and 0x3FFF
	int SPI_output;         // the bit sequence to control the ADC's CS input
	int SPI_clock_mode;     // bit[0] = "inverted" = default state of clock, bit[1] = delayed
	int bit_shift;          // in bits. position of ADC signal inside each 32bit block
	int digital_bit_shift;  // in bits. position of digital signal bit inside each 32bit block

	// measurement related
	int frequency;          // raw samples per second
	int downsampling_rate;  // raw samples that will be averaged and saved as one "saved sample"
	int digital;            // digital channels (0 or 1)
	int filename_counter;   // each file name must have a different name, this will determine the next file name
	int unit_range_choice;  // unused. chooses a or b as the measurement range (0 or 1)
};

struct global_config_struct default_global_config = {"unnamed device", "A", 1, 0, 1, 0, 0, 0x3FFF, 0xFFFF0000, 0, 0, 0, 122070, 2, 0, 0, 0};
struct global_config_struct         global_config;


// starts a measurement and starts the streaming process into a file
void cat_start_next_id(void)
{
	int id = global_config.filename_counter++;
	storage_save_structure("global_config", global_config);
	cat_start(id);
}


// the broadcast thread polls this variable and will not emit a broadcast if this is false
volatile broadcast_enabled = true;

// the broadcast thread periodically sends a UDP packet with the device's current IP address,
// it also polls the physical button and starts/stops the measurememnt when the button is pressed
void *broadcast_thread_entry(void *arg)
{
    //pthread_t id = pthread_self();

	// remember last button press state, to detect on-press events
	int was_pressed = true;
	int button_cooldown = 0; // do not allow button to be pressed too often too quickly
	                         // this prevents the button to start and then immediatelly stop
							 // the measurement

	char buffer[64]; // the data that will be broadcast
	int counter = 0;
	while(true)
	{
		// the thread loops ten times per second
		msleep(100);

		if(button_cooldown > 0)
			button_cooldown--;

		// check if button is pressed
		{
			// choose pin 14
			pgdriver_send("gpio", "\x0E\x02", 2);

			// read the value from the pin
			// invert the value since the button is connected to a pull up resistor
			int is_pressed = !(pgdriver_get_small("gpio", 1) & 1);

			//   detect press events  and  do not interfere with dry_run
			if(!was_pressed && is_pressed && button_cooldown == 0 && !cat_dry_run)
			{
				printf("button press event\n");

				// start/stop the cat stream
				if(cat_enabled)
					cat_stop();
				else
					cat_start_next_id();

				button_cooldown = 10;
			}
			was_pressed = is_pressed;
		}

		if(++counter >= 10) // broadcast only every second
		{
			counter = 0;

			if(broadcast_enabled)
			{
				sprintf(buffer, "PowerGraph beacon %s", global_config.device_name);
				broadcast(buffer, 18 + 32);
			}
		}
	}

	return NULL;
}


// this function is called for each record file. It uploads the file with the given
// filename and then deletes the file
char record_read_buffer[256];
char record_file_path[64];
const char char_one = 1;
void each_record_filename(char* filename)
{
	// skip the . and .. files
	if(filename[0] == '.')return;

	// send notification over the TCP connection, that more files are available
	server_send_bytes(1, &char_one);

	// debug message
	printf("uploading file: %s\n", filename);

	// always send names as 32 bit fixed size strings.
	sprintf(record_file_path, "%s", filename); // pad the string
	server_send_bytes(32, record_file_path);   // send the padded string

	// construct the full file path
	sprintf(record_file_path, "./records/%s", filename);

	// open the record file
	FILE *f = fopen(record_file_path, "rb");
	if(!f)
	{
		printf("error opening file %s\n", record_file_path);
		return;
	}

	// determine the record file size
	fseek(f, 0, SEEK_END);
	int record_file_size = ftell(f);

	// send the file size over the TCP connection
	fseek(f, 0, SEEK_SET);
	server_send_bytes(4, &record_file_size);

	// upload the file by reading and sending blocks of data
	while(!feof(f))
	{
		// read a block of data
		int bytes_read = fread(record_read_buffer, 1, 256, f);

		// end of file?
		if(bytes_read <= 0)
			break;

		// send the data
		server_send_bytes(bytes_read, record_read_buffer);
	}
	fclose(f);
}


int main(int argc, char *argv[])
{
	// if the kernel module is loaded, it will have created the "/dev/pg_stream" device file
	if(access("/dev/pg_stream", R_OK) != 0)
	{
		printf("ERROR driver needs to be loaded before starting the host\n");
		return;
	}

	// load config
	global_config = default_global_config;
	storage_load_structure("global_config", global_config);
	pgdriver_send("config", &global_config, sizeof(global_config));
	printf("starting.  device name: %s\n", global_config.device_name);

	// create the folder, just in case
	mkdir("records");

	// start the broadcast
	pthread_t thread_id;
	if(pthread_create(&(thread_id), NULL, &broadcast_thread_entry, NULL))
	{
        printf("ERROR creating thread\n");
	}

	// wait for connections, forever
	while(true)
	{
		broadcast_enabled = true;
		msleep(1000); // prevent cpu exhaust

		// wait for a connection
		server_accept_connection();
		if(!server_is_connected)
		{
			continue;
		}

		// debug output
		printf("=============== new connection\n");

		// we dont need to broadcast the IP address anymore
		broadcast_enabled = false;

		// handle each received command
		while(server_is_connected)
		{
			// read the command byte, which is always the first byte of every command data block
			char command;
			server_receive_bytes(1, &command);
			if(!server_is_connected) break;

			// debug output
			printf("cmd: %d\n", (int)command);

			if(command == 0xFF)  // command: quit
				break;

			if(command == 1) // command: download records
			{
				// upload all files
				enumerate_records(&each_record_filename);

				// send the byte indicating that no more files are available
				command = 0;
				server_send_bytes(1, &command);
				printf("enumeration complete\n");

				continue;
			}

			if(command == 2) // command: delete record (successfully downloaded)
			{
				char filename[32];
				server_receive_bytes(32, filename); // receive file name to delete
				printf("deleting: %s\n", filename);
				sprintf(record_file_path, "./records/%s", filename);

				// delete
				if(unlink(record_file_path) < 0)
				{
					printf("delete failed. %s\n", record_file_path);
				}

				continue;
			}

			if(command == 3) // command: get config
			{
				server_send_bytes(sizeof(global_config), &global_config);

				continue;
			}

			if(command == 4) // command: set config
			{
				server_receive_bytes(sizeof(global_config), &global_config);

				printf("new config loaded\n");
				printf("device_name       = %s\n", global_config.device_name);
				printf("unit_name         = %s\n", global_config.unit_name);
				printf("unit_significand  = %d\n", global_config.unit_significand_a);
				printf("unit_exponent     = %d\n", global_config.unit_exponent_a);
				printf("ADC_min_value     = %04X\n", global_config.ADC_min_value);
				printf("ADC_max_value     = %04X\n", global_config.ADC_max_value);
				printf("SPI_output        = %08X\n", global_config.SPI_output);
				printf("SPI_clock_mode    = %d\n", global_config.SPI_clock_mode);
				printf("bit_shift         = %d\n", global_config.bit_shift);
				printf("digital_bit_shift = %d\n", global_config.digital_bit_shift);
				printf("\n");
				printf("frequency         = %d\n", global_config.frequency);
				printf("downsampling_rate = %d\n", global_config.downsampling_rate);
				printf("digital           = %d\n", global_config.digital);
				printf("filename_counter  = %d\n", global_config.filename_counter);
				printf("unit_range_choice = %d\n", global_config.unit_range_choice);
				printf("\n");
				storage_save_structure("global_config", global_config);
				pgdriver_send("config", &global_config, sizeof(global_config));

				continue;
			}

			if(command == 7) // start measurement
			{
				cat_start_next_id();

				continue;
			}

			if(command == 8) // get status
			{
				// is the measurement currently running?
				char v = !!cat_enabled;
				server_send_bytes(1, &v);

				continue;
			}

			if(command == 9) // stop measurement
			{
				cat_stop();

				continue;
			}

			if(command == 10) // start dry run.   (measurement without saving data)
			{
				cat_start(-1);

				continue;
			}

			if(command == 11) // get raw SPI 32-bit block
			{
				int raw = pgdriver_get_small("config", 4);
				server_send_bytes(4, &raw);

				continue;
			}

			printf("unknown command\n");
		}
	}

	printf("the end\n");

	return 0;
}
