
import GUI
import socket
import struct
import time
import sys
import math

import record_folder
import dialogs
from copy import copy


PORT = 9657

SIZE_FORMAT = '<I'
NAME_FORMAT = '<32s'
GLOBAL_CONFIG_FORMAT = '<32s16sIiIiIIIIIIIIIII'


# create python string from C string
def c_string(null_terminated_bytes):
	return null_terminated_bytes[:null_terminated_bytes.find(b'\x00')].decode()



def format_significand_exponent(sig, exp):
	if sig == 0:
		return 0;
	sig_span = int(math.log10(sig))
	try:
		top_exp = exp+sig_span
		eng_exp = math.floor((top_exp)/3)
		suffix = "pnum kMGT"[eng_exp + 4]
		factor = 10 ** (exp - eng_exp * 3)
	except:
		suffix = " * 10^{}".format(exp)
		factor = 1
	num_part = sig*factor
	if type(num_part) == float:
		return "{:.4}{}".format(num_part, suffix)
	else:
		return "{}{}".format(num_part, suffix)


def read_format_significand_exponent(f):
	# find index of first non-numeric character
	i = next(i for i,v in enumerate(f+"x") if v not in "0123456789.")
	if i == len(f):
		exp = 0
	else:
		suffix = f[i]
		exp = ("pnum kMGT".index(suffix)-4)*3
	num_part = f[:i]
	if "." in num_part:
		point_pos = f.index(".")
		exp -= len(num_part)-1-point_pos
		num_part = num_part.replace(".", "")
	return (int(num_part), int(exp))
	
	


class ConnectionRefusedError(Exception):
	def __init__(self, message=""):
		Exception.__init__(self, message)
	def __str__(self):
		return "Fail to connect to host.\n{}".format(Exception.__str__(self))




class AdvancedConnection:
	def __init__(self, hostname):
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			self.s.connect((hostname, 9657))
		except Exception as e:
			raise ConnectionRefusedError(str(e))

	def receive(self, byte_count):
		data = b''

		bytes_left = byte_count

		while bytes_left > 0:
			chunk = self.s.recv(bytes_left)

			if chunk == b'':
				raise ValueError("connection reset, no value receiving possible")

			bytes_left -= len(chunk)
			data += chunk
		
		return data

	def receive_struct(self, type):
		unpacked = struct.unpack(type, self.receive(struct.calcsize(type)) )

		if len(unpacked) == 1:
			return unpacked[0]
		return unpacked

	def send(self, data):
		self.s.sendall(data)

	def send_struct(self, stype, data):
		if type(data) != tuple:
			data = (data,)
		self.s.sendall(struct.pack(stype, *data))

	def send_variable_buffer(self, data):
		self.send_struct(SIZE_FORMAT, len(data))
		self.s.sendall(data)

	def receive_variable_buffer(self, chunk_callback=None, size_callback=None, max_bytes=None, chunk_size=16384):
		buffer_length = self.receive_struct(SIZE_FORMAT)

		# notify caller of buffer size
		if size_callback is not None:
			size_callback(buffer_length)

		# if the variable buffer is too big, there's an error
		if max_bytes is not None:
			if buffer_length > max_bytes:
				raise ValueError("the buffer is bigger than allowed by caller ({}bytes > {}bytes)".format(buffer_length, max_bytes))
	
		# if no callback is given, just receive everything in 1 buffer and return the whole buffer
		if chunk_callback is None:
			return self.receive(buffer_length)

		# if callback is given, call it for each received data chunk
		bytes_left = buffer_length

		chunk_buffer = bytearray(chunk_size)  # reuse the same chunk of memory for receiving
		chunk_view = memoryview(chunk_buffer)

		while bytes_left > 0:
			bytes_received = self.s.recv_into(chunk_view[:bytes_left])
			chunk_callback(chunk_view[:bytes_received])

			bytes_left -= bytes_received

	def close(self):
		self.s.shutdown(socket.SHUT_RDWR)



class PowerGraphConnection:
	def __init__(self, hostname):
		self.connection = AdvancedConnection(hostname)

	# handler.next_file(filename) handler.next_chunk(bytes)
	def download_all_records(self, size_callback=lambda size: 0, progress_callback=lambda progress: 0):
		c = self.connection
		c.send(b'\x01')  # protocol

		new_files = []

		while c.receive(1) == b'\x01':
			filename = c_string(c.receive_struct(NAME_FORMAT))
			print("downloading file: {}".format( filename ))
			new_files.append(filename)
			f = record_folder.open_record_file( filename, for_writing=True )

			progress = 0
			def next_chunk(chunk):
				nonlocal progress
				f.write(chunk)
				progress += len(chunk)
				progress_callback(progress)
			def next_size(size):
				size_callback(size)
			
			c.receive_variable_buffer( next_chunk, next_size )
			f.close()
		
		return new_files
	
	def delete_record(self, name):
		c = self.connection
		c.send(b'\x02')
		c.send_struct(NAME_FORMAT, name.encode())

	def start_measurement(self):
		c = self.connection
		c.send(b'\x07')

	def start_dry_run(self):
		c = self.connection
		c.send(b'\x0A')

	def is_measurement_running(self):
		c = self.connection
		c.send(b'\x08')
		return (c.receive_struct('<b') != 0)

	def stop_measurement(self):
		c = self.connection
		c.send(b'\x09')

	def get_raw_SPI_block(self):
		c = self.connection
		c.send(b'\x0B')
		return c.receive_struct('<I')

	def get_config(self):
		c = self.connection
		class config:
			pass
		
		c.send(b'\x03')
		global_config_data = c.receive_struct(GLOBAL_CONFIG_FORMAT)
		#print(global_config_data)

		config.device_name, config.unit_name, config.unit_significand, config.unit_exponent, ignore, ignore, config.ADC_min, config.ADC_max, config.SPI_output_pattern, config.SPI_clock_mode, config.bit_shift, config.digital_bit_shift, config.frequency, config.compound_count, config.digital, config.filename_counter, config.unit_range_choice = global_config_data
		config.device_name = c_string(config.device_name)
		config.unit_name = c_string(config.unit_name)

		return config

	def set_config(self, config):
		c = self.connection

		global_config_data = (config.device_name.encode(), config.unit_name.encode(), config.unit_significand, config.unit_exponent, 1, 0, config.ADC_min, config.ADC_max, config.SPI_output_pattern, config.SPI_clock_mode, config.bit_shift, config.digital_bit_shift, config.frequency, config.compound_count, config.digital, config.filename_counter, config.unit_range_choice)
		
		# test packing, so an exception can be raised before the command byte is send
		struct.pack(GLOBAL_CONFIG_FORMAT, *global_config_data)

		c.send(b'\x04')
		c.send_struct(GLOBAL_CONFIG_FORMAT, global_config_data)

	def close(self):
		c = self.connection
		c.send(b'\xFF')
		c.close()





def open_gui(tk_parent):
	host = choose_host(tk_parent)
	if host is not None:
		connect(tk_parent, host)



def choose_host(tk_parent):
	# create window
	window = GUI.Toplevel(tk_parent)
	window.wm_title("Connect...")
	window.iconbitmap("favicon.ico")
	window.transient(tk_parent)
	window.resizable(0,0)
	window.lift()
	window.grab_set()
	window.focus_set()

	# listbox for display of raspberry-pi devices found in local network
	listbox = GUI.Listbox(window)
	listbox.pack(fill='both', expand=1, padx=2, pady=2)

	# edit box for input of custom address
	editbox = GUI.EditBox(window)
	editbox.configure(state='active')
	editbox.set_cursor(0)
	editbox.pack(side='left', fill='x', expand=1)

	# load initial value from last time
	try:
		with open("saved_address.txt", 'r') as address_file:
			editbox.value = address_file.read()
	except:
		pass


	# button to connect to custom input
	custom_go = GUI.Button(window, text="Connect")
	custom_go.pack(side='left')

	GUI.center_window(window, tk_parent)


	# keep track of beacons (name, host)
	beacons = []

	listbox.delete(0, GUI.END)
	def add_beacon(device_name, host):
		if host not in beacons:
			beacons.append(host)
			listbox.insert(GUI.END, "{} - {}".format(device_name, host))

	# start listening for UDP broadcasts
	global broadcast_socket
	broadcast_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	broadcast_socket.setblocking(0)
	broadcast_socket.bind(('', PORT)) # listen to every network interface

	def tick():
		try:
			while True:
				data, (host, port) = broadcast_socket.recvfrom(256)
				if data.startswith(b'PowerGraph beacon '):
					data = data[18:]
					device_name = data[:data.find(b'\x00')].decode()
					add_beacon(device_name, host)
				else:
					print("weird broadcast message received on correct port")
		except BlockingIOError:
			tick.timer_id = window.after(500, tick)
		except OSError:
			print("unexpected: received message that was too big\n")
	def tick_stop():
		window.after_cancel(tick.timer_id)
		broadcast_socket.close()
	tick()


	# handle listbox click events
	def on_select(e):
		select_index = listbox.curselection()
		if len(select_index) > 0:
			editbox.value = beacons[int(select_index[0])]
	listbox.bind('<<ListboxSelect>>', on_select)


	# on button click
	def on_click(*discard):
		if editbox.value != "":
			# save user's choice so we can load it next time
			with open("saved_address.txt", 'w') as address_file:
				address_file.write(editbox.value)

			on_click.result = editbox.value
			window.destroy()
	on_click.result = None
	custom_go.configure(command=on_click)
	listbox.bind('<Double-Button-1>', on_click)

	# redraw everything
	tk_parent.wait_window(window)
	tick_stop()
	print("user choice: {}".format(on_click.result))

	return on_click.result


def download(tk_parent, connection):
	c = connection
	with dialogs.ProgressDialog(tk_parent, describtion="Downloading...") as progress:
		new_files = c.download_all_records(progress.set_maximum, progress.set_progress)

	for filename in new_files:
		c.delete_record(filename)







def configure_hardware(tk_parent, connection):
	from signal_diagram import SignalDiagram
	c = connection

	# we need a dry run. stop any existing measurements
	c.stop_measurement()
	c.start_dry_run()

	# create window
	window = GUI.Toplevel(tk_parent)
	window.wm_title("Raspberry Control")
	window.iconbitmap("favicon.ico")
	window.transient(tk_parent)
	window.resizable(0,0)
	window.lift()
	window.grab_set()
	window.focus_set()

	# fill GUI line by line
	def next_row():
		next_row.row += 1
		return next_row.row
	next_row.row = -1
	
	# creating settings widgets line by line
	def add_labeled_widget(new_widget, label):
		row = next_row()
		GUI.Label(window, text=label).grid(row=row, column=0, sticky=GUI.align_option('right'))
		new_widget                   .grid(row=row, column=1, sticky=GUI.align_option('left', 'right'))
		return new_widget

	w_device_name   = add_labeled_widget(GUI.EditBox(window), "device name:")
	w_unit_name     = add_labeled_widget(GUI.EditBox(window), "physical unit name (A):")
	w_unit_range    = add_labeled_widget(GUI.EditBox(window), "physical unit for ADC max (e.g. 120m):")
	w_sample_rate   = add_labeled_widget(GUI.ComboBoxEx(window), "samples per second:")
	w_ADC_min       = add_labeled_widget(GUI.EditBox(window), "ADC min value (hex):")
	w_ADC_max       = add_labeled_widget(GUI.EditBox(window), "ADC max value (hex):")
	w_SPI_output    = add_labeled_widget(GUI.EditBox(window), "SPI output pattern (hex):")
	w_SPI_clock_invert = add_labeled_widget(GUI.CheckBox(window), "SPI clock inverted")
	w_SPI_clock_delay  = add_labeled_widget(GUI.CheckBox(window), "SPI clock delayed")
	w_bit_shift     = add_labeled_widget(GUI.EditBox(window), "ADC data bit position:")
	w_digital_shift = add_labeled_widget(GUI.EditBox(window), "digital channel bit position:")

	# load config from tool and display it in the dialog
	config = c.get_config()
	w_device_name.value   = config.device_name
	w_unit_name.value     = config.unit_name
	w_unit_range.value    = format_significand_exponent(config.unit_significand, config.unit_exponent)
	w_sample_rate.values  = ['488281', '244140', '122070', '61035', '30517', '15258', '7629', '3814', '1907', '953']
	w_sample_rate.value   = config.frequency
	w_ADC_min.value       = "{:04X}".format(config.ADC_min)
	w_ADC_max.value       = "{:04X}".format(config.ADC_max)
	w_SPI_output.value    = "{:08X}".format(config.SPI_output_pattern)
	w_SPI_clock_invert.value = (config.SPI_clock_mode & 0x1) != 0
	w_SPI_clock_delay.value  = (config.SPI_clock_mode & 0x2) != 0
	w_bit_shift.value     = config.bit_shift
	w_digital_shift.value = config.digital_bit_shift

	# create diagram
	diagram = SignalDiagram(window, config)
	diagram.grid(row=0, column=3, rowspan=next_row(), sticky='wens')

	# update diagram with live data
	def tick():
		raw = c.get_raw_SPI_block()
		#print("{:08X}".format(raw))
		diagram.live_data = raw
		diagram.redraw()
		diagram.update()
		tick.timer = tk_parent.after(300, tick)
	tick()

	# completion
	def apply_action():
		c.stop_measurement()
		with dialogs.ProgressDialog(window, "Please wait..") as dialog:
			dialog.set_maximum(2)
			dialog.set_progress(0)
			# this simple statement will wait for Raspberry to stop the measurement, which can take a while depending on the configuration
			c.is_measurement_running()
		try:
			new_config                     = copy(config)
			current_entry = "device name"
			if len(w_device_name.value) > 31:
				raise ValueError("too long")
			new_config.device_name         = w_device_name.value
			current_entry = "unit name"
			if len(w_unit_name.value) > 15:
				raise ValueError("too long")
			new_config.unit_name           = w_unit_name.value
			current_entry = "unit range"
			new_config.unit_significand, config.unit_exponent = read_format_significand_exponent(w_unit_range.value)
			current_entry = "sample rate"
			new_config.frequency           = int(w_sample_rate.value)
			current_entry = "ADC min"
			new_config.ADC_min             = int(w_ADC_min.value, 16)
			current_entry = "ADC max"
			new_config.ADC_max             = int(w_ADC_max.value, 16)
			current_entry = "SPI clock mode"
			new_config.SPI_output_pattern  = int(w_SPI_output.value, 16)
			current_entry = "SPI clock mode"
			new_config.SPI_clock_mode      = (1 if w_SPI_clock_invert.value else 0) + (2 if w_SPI_clock_delay.value else 0)
			current_entry = "ADC bit position"
			new_config.bit_shift           = int(w_bit_shift.value)
			current_entry = "digital channel position"
			new_config.digital_bit_shift    = int(w_digital_shift.value)
			current_entry = "unknown"
			c.set_config(config)
			diagram.config = new_config
			c.start_dry_run()
		except (ValueError, struct.error) as e:
			GUI.messagebox.showwarning("Error", "Invalid entry in {}\n{}".format(current_entry, str(e)))
			return
	frame = GUI.Frame(window)
	GUI.Button(window, text="Apply Changes", command=apply_action).grid(column=0, row=next_row(), columnspan=2)

	# show window
	GUI.center_window(window, tk_parent)
	tk_parent.wait_window(window)
	tk_parent.after_cancel(tick.timer)

	c.stop_measurement()




def connect(tk_parent, host):
	try:
		c = PowerGraphConnection(host)
	
		# create window
		window = GUI.Toplevel(tk_parent)
		window.wm_title("Raspberry Control")
		window.iconbitmap("favicon.ico")
		window.transient(tk_parent)
		window.resizable(0,0)
		window.lift()
		window.grab_set()
		window.focus_set()

		# buttons
		frame = GUI.Frame(window)
		GUI.Button(frame, text="Configure Hardware", command=lambda: configure_hardware(window, c)).pack(side='left', padx=2, pady=2)
		GUI.Button(frame, text="Download Records", command=lambda: download(window, c)).pack(side='left', padx=2, pady=2)
		frame2 = GUI.Frame(frame, relief='sunken', borderwidth=0)
		GUI.Button(frame2, text="Start Measurement", command=lambda: c.start_measurement()).pack(padx=2, pady=2)
		GUI.Button(frame2, text="Stop Measurement", command=lambda: c.stop_measurement()).pack(padx=2, pady=2)
		frame2.pack(side='top', padx=2, pady=2)
		frame.grid(row=0, columnspan=2)

		# creating settings widgets line by line
		def add_labeled_widget(new_widget, label):
			GUI.Label(window, text=label).grid(row=add_labeled_widget.grid_row, column=0, sticky=GUI.align_option('right'))
			new_widget.grid(row=add_labeled_widget.grid_row, column=1, sticky=GUI.align_option('left', 'right'))
			add_labeled_widget.grid_row += 1
			return new_widget
		add_labeled_widget.grid_row = 1

		w_downsampling     = add_labeled_widget(GUI.EditBox(window), "combine n samples into 1 (downsampling):")
		w_resulting_rate   = add_labeled_widget(GUI.Label(window), "resulting sample rate:")
		w_filename_counter = add_labeled_widget(GUI.EditBox(window), "filename counter:")

		# load config from tool and display it in the dialog
		config = c.get_config()
		w_downsampling.value     = config.compound_count
		w_resulting_rate.value   = config.frequency//config.compound_count
		w_filename_counter.value = config.filename_counter

		# apply button
		def apply_settings():
			try:
				downsampling = int(w_downsampling.value)
				if downsampling < 1:
					raise ValueError("downsampling value must be 1 or higher")
				config.compound_count = downsampling
				config.filename_counter = int(w_filename_counter.value)
				c.set_config(config)
			except Exception as e:
				GUI.messagebox.showwarning("Error", str(e))
		GUI.Button(window, text="Apply Settings", command=apply_settings).grid(row=add_labeled_widget.grid_row, column=0, columnspan=2, padx=2, pady=2)

		# show window
		GUI.center_window(window, tk_parent)
		tk_parent.wait_window(window)

		c.close()

	except ConnectionRefusedError as e:
		GUI.messagebox.showwarning("Error", str(e))
		return
	except Exception as e:
		GUI.messagebox.showwarning("Error", str(e))
		return







if __name__ == '__main__':
	# this is a pretty self-sustained module
	tk = GUI.Tk()
	tk.update()


	open_gui(tk)

	if False:
		s.send(b'\x04')
		s.send_struct(GLOBAL_CONFIG_FORMAT, (b'unnamed device', b'A', 1,  0, 0, 16383, 4294901760, 0, 32768, 2, 0))
