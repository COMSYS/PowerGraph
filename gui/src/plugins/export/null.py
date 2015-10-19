
from tkinter.messagebox import showinfo



#
# sample_source
#    .goto_sample_index(index)       #
#    .read()                         # returns sample as integer, raises EOFError if no more samples available
#    .header
#       .units_per_measuring_range   # e.g. watt / full_integer_range
#       .full_integer_range          # discrete max value
#       .unit_name                   # without modifier. A instead of mA
#       .samples_per_second          # frequency
#       .digital_channel_count       # 1
#       .sample_count                #
#

def export(sample_source, ProgressDialog):
	count = 0
	try:
		with ProgressDialog() as progress:
			progress.set_maximum(sample_source.header.sample_count)
			while True:
				for i in range(1000):
					count += 1
					sample_source.read()
				progress.add_progress(1000)
	except EOFError:
		pass

	showinfo("success!", "successfully ignored all {} samples".format(count))

