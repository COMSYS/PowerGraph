
import array


# A compound sample is a tuple (min, avg, max) representing one sample of a plot, that consists of multiple normal samples combined.

class CompoundSampleFromStream:
	''' wraps a normal sample stream, and provides a compound sample stream. '''
	samples_per_block = 1       # per definition, theres one sample per compound block here
	stream = 0
	buffer = []                 # for efficient calculations

	def __init__(self, stream):
		self.stream = stream

	def get_next_compound(self, sample_count):
		if len(self.buffer) != sample_count:
			self.buffer = array.array('H', [0]*sample_count)

		# efficient read
		bytes_read = self.stream.byte_stream.readinto(self.buffer)
		#self.buffer.byteswap()

		if bytes_read//2 != sample_count:
			raise EOFError()

		self.stream.sample_index += sample_count

		return (min(self.buffer) >> 1, int(sum(self.buffer) / sample_count) >> 1, max(self.buffer) >> 1)

	def get_sample_index(self):
		return self.stream.get_sample_index()

	def goto_sample_index(self, new_pos):
		self.stream.goto_sample_index(new_pos)

	def get_sample_upper_limit(self):
		return self.stream.get_sample_upper_limit()


class CompoundSampleMipMap:
	samples_per_block = 1       # mipmap step, how many samples will be compressed into one
	blocks = []                 # array of (min, avg, max)
	head = 0

	def __init__(self, samples_per_block):
		self.samples_per_block = samples_per_block

	def generate_from(self, source, progress_callback=None):
		# sets how often the report function will be called
		# calling the report function rarely increases productivity
		report_progress_every_n_loops = 1024

		# reset data
		self.blocks = []

		try:
			while True:
				# before reporting progress, first process some data
				for i in range(report_progress_every_n_loops):
					self.blocks.append(source.get_next_compound(self.samples_per_block))

				# now report progress
				if progress_callback is not None:
					progress_callback(source.get_sample_index())
		except EOFError:
			pass

	def get_next_compound(self, sample_count):
		try:
			# calculate first block index
			block_index = int(self.head // self.samples_per_block)

			# keep track of current position
			self.head += sample_count

			# how many mipmap stored samples will be compressed in this call
			block_span = sample_count // self.samples_per_block

			# fetch first block
			min, avg, max = self.blocks[block_index]

			# keep track how many samples are summed up
			avg_count = 1

			# fetch the rest
			for i in range(block_span):
				block_index += 1
				next_min, next_avg, next_max = self.blocks[block_index]

				# add this block to the bunch
				avg = next_avg + avg
				avg_count += 1
				if next_min < min:
					min = next_min
				if next_max > max:
					max = next_max

			# calculate the average
			avg //= avg_count

			return (min, avg, max)

		except IndexError:
			raise EOFError

	def goto_sample_index(self, new_pos):
		self.head = new_pos

	def get_sample_index(self):
		return self.head


# a container for mipmaps
class MipMapList:
	def __init__(self):
		self.mipmaps = []

	def add_level(self, new_level):
		self.mipmaps.append(new_level)

	# scan the levels and return the mipmap level most fitting the desired scale
	def get_level(self, desired_scale):
		best_index = None
		for i in range(len(self.mipmaps)):
			i_scale    = self.mipmaps[i].samples_per_block
			if best_index is None:
				if i_scale <= desired_scale:
					best_index = i
			else:
				best_scale = self.mipmaps[best_index].samples_per_block
				if i_scale <= desired_scale and i_scale > best_scale:
					best_index = i

		#print("using mipmap level: {} for {}".format(self.mipmaps[best_index].samples_per_block, desired_scale))
		return self.mipmaps[best_index]

