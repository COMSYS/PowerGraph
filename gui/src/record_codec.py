import struct
import io

# sample record format
# header:
#   char[18] magic string (const) 'power graph record' 
#   char[16] suffix of unit of measurement in ASCII without exponent symbol. For example 'A' instead of 'mA' and 'V' instead of 'mV'. rest are zero bytes
#   uint16   physical unit amount for full ADC range. first 4 numbers in decimal system
#   sint8    exponent in decimal representation
#   uint32   frequency in samples/second. always constant throughout the record
#   uint8    analog signal count
#   uint8    digital signal count
#   uint16   analog signal max integer value (=value range)
#   char[7]  reserved for future use (make it zero)
#
# header size 52 bytes. sample count is predictable
#
# each sample:
#   uint16   lowest bits are analog data. highest bits are digital data. e.g.: d0aaaaaa aaaaaaaa
#


# represents the settings of a record. Immutable, relates to one specific sample source
class Header:
	def __init__(self):
		self.units_per_measuring_range = 1    # e.g. watt / full_integer_range
		self.full_integer_range = 1           # discrete max value
		self.unit_name   = ''                 # without modifier. A instead of mA
		self.samples_per_second = 0           # frequency
		self.digital_channel_count = 0        #
		self.sample_count = 1


# checks whether number is representable as an n-bit signed integer
def _is_n_bits(number, bit_count):
	min = -1 << (bit_count-1)
	max = (1 << (bit_count-1)) - 1
	return (min <= number <= max)
def _assert_is_n_bits(number, bit_count):
	if not _is_n_bits(number, bit_count):
		raise ValueError("argument {} out of range needs to fit into {} bytes".format(number, bit_count))
def _signed_value_of_n_bits(bits, bit_count):
	sign_mask = 1 << (bit_count-1)
	mask = sign_mask - 1
	is_minus = bits & sign_mask
	if is_minus:
		return -((~bits) & mask)-1
	else:
		return bits & mask


class StreamEncoder:
	''' Outputs a stream in a valid pgrecord file format, when given data vie function calls. '''

	byte_stream = 0
	last_sample = 0
	last_digital = 0

	# only one of these can be not-null at any given time
	queued_delta = None
	repeated_sample = 0

	def __init__(self, byte_stream):
		self.byte_stream = byte_stream

	# record must always start like this:
	def write_header(self, unit_name, samples_per_second, digital_signal_count, full_integer_range, measurement_range):
		self.byte_stream.write(b'power graph record')
		self.byte_stream.write(struct.pack('>16s', unit_name.encode('ascii')))
		def calc_mantrissa_exponent(n, base):
			exponent = 0
			while True:
				if n >= base:
					n /= base
					exponent += 1
				elif n < 1:
					n *= base
					exponent -= 1
				else:
					return n, exponent
		significant, exponent = calc_mantrissa_exponent(measurement_range, 10)
		self.byte_stream.write(struct.pack('<H', round(significant*1000)))
		self.byte_stream.write(struct.pack('<b', exponent-3))
		self.byte_stream.write(struct.pack('<I', samples_per_second))
		self.byte_stream.write(b'\x01')
		self.byte_stream.write(struct.pack('<B', digital_signal_count))
		self.byte_stream.write(struct.pack('<H', full_integer_range))
		self.byte_stream.write(b'\x00'*7)

		self.full_integer_range = full_integer_range


	def write_sample(self, sample):
		if sample > self.full_integer_range:
			sample = self.full_integer_range
		if sample < 0:
			sample = 0
		self.byte_stream.write(struct.pack('<H', sample << 1))




class StreamDecoder:
	''' Reads a file in a valid pgrecord format. '''

	byte_stream = 0                  # must have .read()
	sample_index = 0

	header = None

	def __init__(self, byte_stream):
		self.byte_stream = byte_stream
		if b'power graph record' != self.byte_stream.read(18):
			raise ValueError("Record does not start with the correct magic string.")
		self.header = Header()
		self.header.unit_name             = self.unpack('>16s').decode('ascii').rstrip('\x00')
		mantissa = self.unpack("<H")
		exponent = self.unpack("<b")
		self.header.units_per_measuring_range = mantissa*(10**exponent)  # base 10 exponent
		self.header.samples_per_second    = self.unpack('<I')
		self.unpack('<B')                                                # assume one analog channel, ignore this byte
		self.header.digital_channel_count = self.unpack('<B')
		self.header.full_integer_range    = self.unpack('<H')

		self.byte_stream.read(7)  # ignore the 7 reserved bytes

		# at this point the header is fully read.
		self.header_size = self.byte_stream.tell()

		# get the file size
		save = self.byte_stream.tell()
		self.byte_stream.seek(0, io.SEEK_END)
		self.file_size = self.byte_stream.tell()
		self.byte_stream.seek(save, io.SEEK_SET)

		# it's now easy to determine sample count
		self.header.sample_count = (self.file_size - self.header_size)/2
	
	def unpack(self, format):
		bytes = self.byte_stream.read(struct.calcsize(format))
		return struct.unpack(format, bytes)[0]

	# returns sample or raises EOFError
	def read(self):
		word = self.byte_stream.read(2)
		if word == b'':
			raise EOFError("PGRecord ended")
		
		# keep count of samples
		self.sample_index += 1

		return (int.from_bytes(word, byteorder='little')  >> 1)

	def get_sample_index(self):
		return self.sample_index

	def goto_sample_index(self, new_index):
		self.byte_stream.seek(self.header_size + int(new_index)*2, io.SEEK_SET)
		self.sample_index = new_index

