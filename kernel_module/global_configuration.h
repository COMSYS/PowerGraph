struct global_config_struct
{
	// general config
	char device_name[32];   // usefull for indentification if you have more than one PowerGraph tool
	char unit_name[16];     // physical measurement unit
	int unit_significand_a; // "unit" per ADC measuring range
	int unit_exponent_a;    // base 10
	int unit_significand_b; // reserved for later, if measurement range switching is to be implemented
	int unit_exponent_b;    //
	int ADC_min_value;      // this ADC value corresponds to measurement zero-value
	int ADC_max_value;      // 0x3FFF
	int SPI_output;         // the bit sequence to control the ADC
	int SPI_clock_mode;     // bit[0] = "inverted" = default state of clock, bit[1] = delayed
	int bit_shift;          // in bits. position of ADC signal inside each 32bit block
	int digital_bit_shift;  // position of digital signal bit inside each 32bit block

	// measurement related
	int sample_rate;        // rawsamples/second
	int downsampling_rate;  // raw samples/saved sample
	int digital;            // digital channels (0 or 1)
	int filename_counter;   // each file name must be different
	int unit_range_choice;  // choose a or b (0 or 1)
};

