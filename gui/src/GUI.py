from tkinter import *
from tkinter.ttk import *
import tkinter.scrolledtext as scrolledtext
import tkinter.messagebox as messagebox
import pprint
import threading

from canvas import Point


# Tkinter, originally based on the Tcl programming language, has a lot of not-Pythonic artefacts and inconsitencies that
# are hidden behind a layer in this file.


# .colorchooser
# .filedialog
# .commondialog
# .simpledialog
# .font
# .dnd
# .scrolledtext




class VariableDependentTkinterWidget:
	'''
	Boilerplate reducing class. Every value dependent widget requires additional code and variables in tkinter, which are autonatically generated here.
	'''

	previous_state = False
	def __init__(self, tkinter_variable_class):
		self.state_tracker = tkinter_variable_class()   # create instance of that class

	@property
	def value(self):
		return self.state_tracker.get()

	@value.setter
	def value(self, new_value):
		self.state_tracker.set(new_value)

	def callback_on_value_change(self, callback):  # callback(value, previous_value)
		def validate(*ignore):
			next_state = self.value
			if self.previous_state != next_state:
				self.previous_state = next_state
				# support both callback styles
				try:
					callback(next_state, self.previous_state)
				except TypeError:
					callback(next_state)
		self.state_tracker.trace('w', validate)


class CheckBox(Checkbutton, VariableDependentTkinterWidget):
	''' Variable dependent Tkinter widget made pythonic, use this instead of Checkbutton. '''

	def __init__(self, *args, **kwargs):
		VariableDependentTkinterWidget.__init__(self, BooleanVar)
		Checkbutton.__init__(self, *args, **kwargs)
		previous_state = False
		self.state_tracker.set(0)
		self.configure(variable=self.state_tracker)


class EditBox(Entry, VariableDependentTkinterWidget):
	''' Variable dependent Tkinter widget made pythonic, use this instead of Entry. '''

	edit_box = None
	def __init__(self, *args, **kwargs):
		VariableDependentTkinterWidget.__init__(self, StringVar)
		Entry.__init__(self, *args, **kwargs) # cannot really inherit, tkinter has a bug and textvariable=? won't work if set here in __init__
		previous_state = ""
		self.state_tracker.set("")
		
		# to bypass the problem, set textvariable a bit later
		self.after(0, lambda: self.configure(textvariable=self.state_tracker))

	@property
	def value(self):
		return self.state_tracker.get()

	@value.setter
	def value(self, new_value):
		self.state_tracker.set(new_value)

	def set_cursor(self, start, end=None):
		''' if no selection needed, don't specify second parameter '''
		if not end:
			self.icursor(start)
		else:
			self.icursor(end)
			self.selection_range(min(start, end), max(start, end))
		
	def callback_on_validate(self, when, callback):
		when_enum = ('key', 'focus', 'focusin', 'focusout', 'all', 'none')
		if when not in when_enum:
			raise ValueError("when must be one of " + str(when_enum))

		vcmd = (self.register(callback), '%P', '%s')

		# this one works with the validate__ below, but it's way too complicated
		#vcmd = (self.register(callback), '%d', '%i', '%P', '%s', '%S', '%v', '%V', '%W')
		self.configure(validate=when)    # when do we validate?  key focus focusin focusout all none
		self.configure(validatecommand=vcmd)


class ComboBoxEx(Combobox, VariableDependentTkinterWidget):
	''' Variable dependent Tkinter widget made pythonic, use this instead of Combobox. '''

	def __init__(self, *args, **kwargs):
		VariableDependentTkinterWidget.__init__(self, StringVar)
		Combobox.__init__(self, *args, **kwargs)
		previous_state = ""
		self.state_tracker.set("")
		self.configure(textvariable=self.state_tracker)

	@property
	def values(self):
		return self.cget('values')

	@values.setter
	def values(self, value_list):
		self.configure(values=value_list)  # set drop down strings
		self.value = value_list[0]


class RadioButtonEx(Radiobutton, VariableDependentTkinterWidget):
	''' Variable dependent Tkinter widget made pythonic, use this instead of Radiobutton. '''

	def __init__(self, *args, **kwargs):
		VariableDependentTkinterWidget.__init__(self, IntVar)
		Radiobutton.__init__(self, *args, **kwargs)
		previous_state = ""
		self.state_tracker.set("")
		self.configure(variable=self.state_tracker)


# valid percent substitutions for callback binder Widget.register function (from the Tk entry man page)
# %d - Type of action (1=insert, 0=delete, -1 for others)
# %i - index of char string to be inserted/deleted, or -1
# %P - value of the entry if the edit is allowed
# %s - value of entry prior to editing
# %S - the text string being inserted or deleted, if any
# %v - the type of validation that is currently set
# %V - the type of validation that triggered the callback
#      (key, focusin, focusout, forced)
# %W - the tk name of the widget
def validate__(type_of_change, index_of_insert, new_text, old_text, insert_text, validation_type, validation_info, widget_name):
	print("OnValidate")
	return ('q' not in new_text) # discard all new q



def align_option(*sides):
	"""
	Construct valid value from 'left', 'right', 'top', 'bottom' for
	- sticky parameter to grid().
	- anchor parameter to place().
	"""
	option = ''
	if 'top' in sides:
		option += 'n'
	if 'bottom' in sides:
		option += 's'
	if 'right' in sides:
		option += 'e'
	if 'left' in sides:
		option += 'w'
	return option




# combine several consecutive calls into one call which is executed slightly later
# used to work around Tkinter problems by compounding callbacks.
class CombineCall:
	function_to_call = None
	tk_timer_id = None
	some_tk_widget = 0
	collect_time = 40
	def __init__(self, some_tk_widget, function_to_call = None, collect_time = 50):
		self.some_tk_widget = some_tk_widget
		self.function_to_call = function_to_call
		self.collect_time = collect_time

	def __call__(self):
		''' once this function is called, the function_to_call will be called in a short while.'''
		if self.function_to_call is not None:
			if self.tk_timer_id is None:
				self.tk_timer_id = self.some_tk_widget.after(self.collect_time, self._execute)
		
	def cancel(self):
		self.tk_timer_id.after_cancel(self.tk_timer_id)

	def _execute(self):
		f = self.function_to_call # fetch the function
		if f is not None:
			f()
		self.tk_timer_id = None


# for each mouse event triggering inside 'widget', a function will be called in 'widget.mouse_handler'
# possible functions in 'mouse_handler':
#  wheel(local_pos)
#  L_start_drag(local_pos)    <-- must return a function L_drag(local_pos)
#  L_release(local_pos)
def bind_mouse_events(widget):
	def mouse_local_position(event):
		return Point(event.x_root - widget.winfo_rootx(), event.y_root - widget.winfo_rooty())

	def on_wheel(event):
		if widget.mouse_handler is not None:
			widget.mouse_handler.wheel(mouse_local_position(event), event.delta)

	def on_drag(event):
		f = on_drag.drag_callback
		if f is not None:
			f(mouse_local_position(event))
	on_drag.drag_callback = None

	def on_press(event):
		if widget.mouse_handler is not None:
			on_drag.drag_callback = widget.mouse_handler.L_start_drag(mouse_local_position(event))

	def on_release(event):
		if widget.mouse_handler is not None:
			widget.mouse_handler.L_release(mouse_local_position(event))

	widget.mouse_handler = None
	widget.bind_all('<MouseWheel>', on_wheel)
	widget.bind('<Button-1>', on_press)  # left mouse button press
	widget.bind('<B1-Motion>', on_drag)  # left mouse button hold+drag
	widget.bind('<ButtonRelease-1>', on_release)  # left mouse button release


# move the target_window into the middle of the relative_window
def center_window(target_window, relative_window):
	target_window.update()
	px = relative_window.winfo_x()
	py = relative_window.winfo_y()
	pw = relative_window.winfo_width()
	ph = relative_window.winfo_height()

	w = target_window.winfo_reqwidth()
	h = target_window.winfo_reqheight()

	x = px + (pw/2) - (w/2) # calculate centered window position
	y = py + (ph/2) - (h/2)

	if x < 0: # do not allow window outside of screen
		x = 0
	if y < 0:
		y = 0

	target_window.geometry('%dx%d+%d+%d' % (w, h, x, y)) # set dimensions of window

