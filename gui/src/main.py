#!/usr/bin/env python

import GUI
from canvas import *

import compound_samples
import record_codec
import record_folder
import visualization
import dialogs
from plugin_system import PluginFolder
from tkinter import filedialog
import tkinter.simpledialog
import communication

import io
import os

import imp

import logo_renderer

import traceback


# the program can only run if the current directory is set to this file's location
os.chdir(os.path.dirname(os.path.realpath(__file__)))


# snaps the window dimensions to a smaller value
def snap(x):
	return int(x/150)*150


class Application:
	gui_window = GUI.Tk()
	canvas_widget = 0
	collected_redraw_call = GUI.CombineCall(gui_window)

	plot = None
	canvas_interface = None
	codec = None

	def __init__(self):
		# stylize the title bar
		self.gui_window.wm_title("Record Viewer")
		self.gui_window.iconbitmap("favicon.ico")
		self.gui_window.geometry('1200x500')
		#self.gui_window.minsize(700, 730)
		self.gui_window.minsize(0, 0)

		# opened file panel
		frame = GUI.Frame(self.gui_window)
		button_open = GUI.Button(frame, text="Open", command=self.open_command)
		button_open.pack(side='left')
		self.button_qopen = GUI.Button(frame, text="Quick Open", command=self.quick_open_command)
		self.button_qopen.pack(side='left')
		self.open_file_display = GUI.EditBox(frame)
		self.open_file_display.configure(state='readonly')
		self.open_file_display.set_cursor(0)
		self.open_file_display.value = ""
		self.open_file_display.pack(side='left', fill='x', expand=1)
		self.button_export = GUI.Button(frame, text="Export Data", command=self.export_menu)
		self.button_export.pack(side='left')
		self.button_export_image = GUI.Button(frame, text="Export Image", command=self.export_image_menu)
		self.button_export_image.pack(side='left')
		button_close = GUI.Button(frame, text="Close", command=self.close_file)
		button_close.pack(side='left')
		frame.pack(fill='x')

		# renderer panel
		frame = GUI.Frame(self.gui_window)
		self.button_scan = GUI.Button(frame, text="Precalculate", command=self.scan_opened_file)
		self.button_scan.pack(side='left')
		self.raw_labels = GUI.CheckBox(frame, text="Raw Labels")
		self.raw_labels.config(command=lambda: self.configure_plot('raw_labels', self.raw_labels.value))
		self.raw_labels.pack(side='left')
		self.snap_size = GUI.CheckBox(frame, text="Snap Size")
		self.snap_size.pack(side='left')
		self.button_connect = GUI.Button(frame, text="Connect", command=lambda: communication.open_gui(self.gui_window))
		self.button_connect.pack(side='right')
		frame.pack(fill='x')

		# graph viewing area
		w = GUI.Canvas(self.gui_window, borderwidth=1, relief='sunken', bg='white')
		w.pack(fill='both', expand=1, padx=2, pady=2)
		self.canvas_widget = w
		self.canvas_interface = CanvasTkinterAdapter(w)

		# redraw request might be issued very often consecutively, so we should collect it
		self.redraw_canvas = GUI.CombineCall(w, self.collected_redraw_canvas)
		self.collected_redraw_call.function_to_call = self.redraw_canvas

		def on_canvas_resize(event):
			# store the dimensions inside the widget itself
			# snap if necessary
			if self.snap_size.value:
				self.canvas_widget.width = snap(event.width)
				self.canvas_widget.height = snap(event.height)
			else:
				self.canvas_widget.width = event.width
				self.canvas_widget.height = event.height

			self.redraw_canvas()
		w.bind('<Configure>', on_canvas_resize)

		# init with preliminary values, in case the system doesn't generate Configure events
		self.canvas_widget.width = 1200
		self.canvas_widget.height = 800

		GUI.bind_mouse_events(w)

		# render empty plot
		self.redraw_canvas()

	def configure_plot(self, config_name, value):
		if self.plot is not None:
			setattr(self.plot, config_name, value)
		self.redraw_canvas()

	# called when window resizes or contents like zoom or gadgets change
	def collected_redraw_canvas(self):
		self.canvas_widget.delete('all')

		if self.plot is None:
			logo_renderer.render(self.canvas_widget)
		else:
			self.plot.render(self.canvas_interface, Point(0, 0), Point(self.canvas_widget.width, self.canvas_widget.height))

		# make the graphic interactive
		if self.plot is not None:
			self.canvas_widget.mouse_handler = self.plot.mouse_event_dispatcher
			self.plot.on_change_callback = lambda: self.redraw_canvas()


	def wait_untill_close(self):
		self.gui_window.mainloop()

	def open_record_file(self, record_file_name):
		self.close_file()

		f = record_folder.open_record_file(record_file_name)
		self.codec = record_codec.StreamDecoder(f)

		# create a new renderer
		mipmaps = compound_samples.MipMapList()
		mipmaps.add_level(compound_samples.CompoundSampleFromStream(self.codec))
		self.plot = visualization.PlotView(mipmaps, self.codec.header)	

		# display the filename in the GUI 
		self.open_file_display.value = f.name

		# render result
		self.redraw_canvas()

		# print some values about this file for curious people
		print("full_integer_range: {:04X}".format(self.codec.header.full_integer_range))
		print("units_per_measuring_range: {}".format(self.codec.header.units_per_measuring_range))
		print("samples_per_second: {}".format(self.codec.header.samples_per_second))

	def close_file(self):
		self.open_file_display.value = ""
		self.codec = None
		self.plot = None
		self.button_scan.configure(state='normal')

		# render result to clear screen from last plot
		self.redraw_canvas()

	def scan_opened_file(self):
		if self.plot is None:
			return

		# disable the button to stop repeating this step
		self.button_scan.configure(state='disabled')   # un-press-able

		# restart the codec from the first sample
		self.codec.goto_sample_index(0)

		# create mip maps
		mipmap_step = 64

		with dialogs.ProgressDialog(self.gui_window, "Indexing...") as dialog:
			dialog.set_maximum(self.codec.header.sample_count)

			# first level mipmap is the file itself
			indexer = compound_samples.CompoundSampleFromStream(self.codec)
			mipmap1 = compound_samples.CompoundSampleMipMap(mipmap_step)
			mipmap1.generate_from(indexer, lambda head: dialog.set_progress(indexer.get_sample_index()))

			# second mipmap level can now be calculated
			mipmap2 = compound_samples.CompoundSampleMipMap(mipmap_step ** 2)
			mipmap2.generate_from(mipmap1, lambda head: dialog.set_progress(mipmap1.get_sample_index()))

			# third mipmap level can now be calculated
			mipmap3 = compound_samples.CompoundSampleMipMap(mipmap_step ** 3)
			mipmap3.generate_from(mipmap2, lambda head: dialog.set_progress(mipmap2.get_sample_index()))

		# create a new renderer
		mipmaps = compound_samples.MipMapList()
		mipmaps.add_level(compound_samples.CompoundSampleFromStream(self.codec))
		mipmaps.add_level(mipmap1)
		mipmaps.add_level(mipmap2)
		mipmaps.add_level(mipmap3)
		self.plot = visualization.PlotView(mipmaps, self.codec.header)

		# render result
		self.redraw_canvas()

	def export_menu(self):
		if self.codec is None:
			return

		folder = PluginFolder("plugins.export")

		menu = GUI.Menu(self.gui_window)
		for module_name in folder.enumerate():
			# what happends on_click
			def on_click():
				try:
					export_plugin = folder.load(module_name)

					# suggest a filename and initial directory. let's hope the plugin uses these values
					default_filename = os.path.basename(self.codec.byte_stream.name)
					if default_filename.endswith(".pgrecord"):
						default_filename = default_filename[:-9]
					default_folder = os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/../exports")

					self.codec.goto_sample_index(0)
					export_plugin.export(self.codec, lambda: dialogs.ProgressDialog(self.gui_window, "asdf"))
				except Exception as e:
					# user cancelled or serious error
					GUI.messagebox.showwarning("Error", str(e))
			menu.add_command(label=module_name, command=on_click)

		# popup menu
		menu.post(self.button_export.winfo_rootx(), self.button_export.winfo_rooty() + self.button_export.winfo_height())

	def export_image_menu(self):
		if self.codec is None:
			return

		folder = PluginFolder("plugins.export_image")

		# create menu
		menu = GUI.Menu(self.gui_window)
		for module_name in folder.enumerate():
			# what happends on_click
			def on_click():
				try:
					export_plugin = folder.load(module_name)
					imp.reload(export_plugin)

					# suggest a filename and initial directory. let's hope the plugin uses these values
					default_filename = os.path.basename(self.codec.byte_stream.name)
					if default_filename.endswith(".pgrecord"):
						default_filename = default_filename[:-9]
					default_folder = os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/../exports")

					pdf_canvas = export_plugin.ExporterCanvas(self.gui_window, default_filename, default_folder)
					self.plot.render(pdf_canvas, Point(0, 0), Point(self.canvas_widget.width, self.canvas_widget.height))
				except Exception as e:
					# user cancelled or serious error
					GUI.messagebox.showwarning("Error", str(traceback.format_exc()))
			menu.add_command(label=module_name, command=on_click)

		# popup menu
		menu.post(self.button_export_image.winfo_rootx(), self.button_export_image.winfo_rooty() + self.button_export_image.winfo_height())

	def import_menu(self):
		folder = PluginFolder("plugins.import")

		menu = GUI.Menu(self.gui_window)
		for module_name in folder.enumerate():
			# what happends on_click
			def on_click():
				try:
					import_plugin = folder.load(module_name)

					# start plugin, give it access to the record codec
					def create_output_file():
						file_name = tkinter.simpledialog.askstring("Save As", "choose file name:")
						if file_name == "":
							raise Exception("user cancelled")
						#return record_codec.StreamEncoder(open("../records/{}.pgrecord".format(file_name), "wb"));
					import_plugin.start(create_output_file)
				except Exception as e:
					# user cancelled or serious error
					GUI.messagebox.showwarning("Error", str(e))
			menu.add_command(label=module_name, command=on_click)

		# popup menu
		menu.post(self.button_export.winfo_rootx(), self.button_export.winfo_rooty() + self.button_export.winfo_height())

	def open_command(self):
		filename = filedialog.askopenfilename(parent=self.gui_window, title="Open...", filetypes=[('Power Graph Record File', ".pgrecord")])
		if filename != "":
			self.open_record_file(filename)

	def quick_open_command(self):
		spawn_coords = (self.button_qopen.winfo_rootx(), self.button_qopen.winfo_rooty() + self.button_qopen.winfo_height())

		filename = record_folder.ask_choose_record(self.gui_window, spawn_coords)
		if filename is not None:
			self.open_record_file(filename)


app = Application()
app.wait_untill_close()
