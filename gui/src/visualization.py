import canvas
from canvas import Point, Rectangle
import itertools
import math

# used to compare floating point values
epsilon = 0.1**10

# return the first element that satisfies the test
def first(test, iter):
	return next(filter(test, iter))



def x_axis_label(value, precision):
	result = ""
	minutes = int(value / 60)
	seconds = value % 60
	milli =   round( (seconds * 1000) % 1000 , 1 )
	seconds =   int( (seconds * 1000) / 1000 )
	if minutes > 0:
		result += "{}m".format(minutes)
	result += "{}s\n{}ms".format(seconds, milli)
	
	return result #"{:.3g}s".format(value)

suffix_type_strings = ["", "m", "u", "n", "p", "f"]

def ceil_by_multiple(value, multiple_of):
	return int(math.ceil(value / multiple_of)) * multiple_of

def y_axis_label(value, precision):
	# generate string
	suffix_type = 0
	if value < epsilon:
		value = 0
	if value > 0:
		while value < 1:
			value *= 1000;
			suffix_type += 1
	return "{:.3g}{}A".format(value, suffix_type_strings[suffix_type])

class MouseEventDispatcher:
	def __init__(self):
		self.handlers = []

	# handler must implement is_inside()
	def add_handler(self, new_handler):
		self.handlers.append(new_handler)

	# redirect any function call to a matching handler
	def __getattr__(self, name):
		# return a callable object
		def func(event_position, *args):
			for handler in self.handlers:
				# skip this handler if the event is outside
				if not handler.rect.is_inside(event_position):
					continue

				# handler must implement this event
				handler_func = getattr(handler, name, None)
				if handler_func is not None:
					retval = handler_func(handler.rect.global_to_local(event_position), *args)

					# modify the drag functions to adjust the coordinate
					# the drag function is called with the same area-local coordinates as the other functions
					if name == 'L_start_drag':
						return lambda pos: retval(handler.rect.global_to_local(pos))
					
					return retval
		return func


class PlotView:
	'''
	Renders the plot and the axis into a Canvas interface. During rendering, the click regions are defined, with interaction callback functions.
	Thus this class defines everything about the plot, without having to deal with Tkinter
	'''

	data_source = 0      # must have get_level()

	view_position = 0    #
	view_zoom = 1        #
	scale_zoom = 1
	scale_position = 0

	mouse_event_dispatcher = None
	on_change_callback = lambda: ()

	raw_labels = False

	def __init__(self, data_source, header):
		self.data_source = data_source
		self.header = header


	# coordinate conversion routines for x axis
	def x_from_sample_number(self, sample_number):
		return (sample_number - self.view_position) // self.view_zoom
	def sample_number_from_x(self, x):
		return int(x*self.view_zoom + self.view_position)
	def value_from_sample_number(self, sample_number):
		return sample_number / self.header.samples_per_second
	def sample_number_from_value(self, value):
		return int(value * self.header.samples_per_second)
	def x_from_value(self, value):
		return self.x_from_sample_number(self.sample_number_from_value(value))
	def value_from_x(self, x):
		return self.value_from_sample_number(self.sample_number_from_x(x))
	def y_from_raw(self, raw):
		return (raw - self.scale_position) * self.scale_zoom
	def raw_from_y(self, y):
		return int(y / self.scale_zoom + self.scale_position)
	def value_from_raw(self, raw):
		return raw * self.header.units_per_measuring_range / self.header.full_integer_range
	def raw_from_value(self, value):
		return int(value * self.header.full_integer_range / self.header.units_per_measuring_range)
	def y_from_value(self, value):
		return self.y_from_raw(self.raw_from_value(value))
	def value_from_y(self, y):
		return self.value_from_raw(self.raw_from_y(y))

	def render(self, target_canvas, canvas_offset, canvas_size):
		target_canvas.begin(canvas_size)
		#x = canvas_offset.x
		#y = canvas_offset.y

		# create interaction callbacks
		class PlotArea:
			def wheel(local_pos, scroll_direction):
				center = local_pos.x

				if scroll_direction < 0:
					self.view_position -= center * self.view_zoom
					self.view_zoom *= 2
				else:
					self.view_zoom //= 2
					self.view_position += center * self.view_zoom
				
				self.on_change_callback()

		class LeftAxis:
			def wheel(local_pos, scroll_direction):
				if scroll_direction < 0:
					self.scale_zoom /= 2
				else:
					self.scale_zoom *= 2
				
				self.on_change_callback()

			def L_start_drag(initial_mouse_pos):
				# store the initial position as a starting point for later
				starting_point = self.scale_position

				# create drag handler function
				def L_drag(local_pos):
					self.scale_position = starting_point + (local_pos.y - initial_mouse_pos.y) / self.scale_zoom
					self.on_change_callback()

				return L_drag

		class BottomAxis:
			def wheel(local_pos, scroll_direction):
				center = local_pos.x

				if scroll_direction < 0:
					self.view_position -= center * self.view_zoom
					self.view_zoom *= 2
				else:
					self.view_zoom //= 2
					self.view_position += center * self.view_zoom
				
				self.on_change_callback()

			def L_start_drag(initial_mouse_pos):
				# store the initial position as a starting point for later
				starting_point = self.view_position

				# create drag handler function
				def L_drag(local_pos):
					self.view_position = starting_point - (local_pos.x - initial_mouse_pos.x) * self.view_zoom
					self.on_change_callback()

				return L_drag

		self.mouse_event_dispatcher = MouseEventDispatcher()
		self.mouse_event_dispatcher.add_handler(PlotArea)
		self.mouse_event_dispatcher.add_handler(LeftAxis)
		self.mouse_event_dispatcher.add_handler(BottomAxis)

		# split render area into segments
		border = 30
		left_margin = 80
		bottom_margin = 50
		plot_height = canvas_size.y - border*2 - bottom_margin
		PlotArea.rect   = Rectangle(Point(border+left_margin, border), Point(canvas_size.x-border, border+plot_height))
		LeftAxis.rect   = Rectangle(Point(border, border), Point(border+left_margin, border+plot_height))
		BottomAxis.rect = Rectangle(Point(border+left_margin, border+plot_height), Point(canvas_size.x-border, border+plot_height+bottom_margin))

		# limit the zoom and scroll interaction
		if self.scale_position < 0:
			self.scale_position = 0
		if self.view_zoom < 1:
			self.view_zoom = 1
		self.view_position = min(self.view_position, self.header.sample_count - canvas_size.x * self.view_zoom)
		self.view_position = max(self.view_position, 0)

		# setup source of data
		compounder = self.data_source.get_level(self.view_zoom)

		# render line of signal
		compounder.goto_sample_index(self.view_position)
		def _apply_scale(value):
			value = (value - self.scale_position) * self.scale_zoom
			if value < 0:
				value = 0
			if value > PlotArea.rect.height:
				value = PlotArea.rect.height
			return value

		# generate compound(min, avg, max) sample list
		compound_samples = []
		x = 0
		try:
			while x < PlotArea.rect.width:
				x += 1
				compound_samples.append( compounder.get_next_compound(self.view_zoom) )
		except EOFError:
			pass

		# extracting data from the list
		def points(type):
			for x, c in enumerate(compound_samples):
				yield Point(x,   _apply_scale(self.raw_from_value(self.value_from_raw(c[type]))))

		plot_canvas = canvas.CanvasTransform(target_canvas, offset=Point(PlotArea.rect.left, PlotArea.rect.bottom), scale=Point(1, -1))

		# draw data
		plot_canvas.pen(dash=False)

		plot_canvas.pen(color=(255, 192, 192), width=1)
		plot_canvas.draw_polygon( itertools.chain(points(0), reversed( list(points(2)) )) )



		# draw grid
		try:
			def float_range(start, end, step):
				while start <= end:
					yield start
					start += step

			# minimal distance between two vertical grid lines
			dx_min = 70

			value_start = self.value_from_x(0)
			value_end   = self.value_from_x(PlotArea.rect.width)
			dvalue_min  = self.value_from_x(dx_min) - value_start
			precision = -int(math.floor(math.log(dvalue_min, 10)))

			# find the best decimal step
			decimal_value_step = 10 ** -precision
			value_step = first(lambda x: x >= dvalue_min, [1*decimal_value_step, 2*decimal_value_step, 5*decimal_value_step, 10*decimal_value_step])
			for value in float_range(round(value_start-0.5, precision-1), value_end, value_step):
				if value < value_start or value > value_end:
					continue

				x = self.x_from_value(value)

				plot_canvas.pen(color=(0, 0, 0), width=0.5, dash=True)
				plot_canvas.draw_line([Point(x, 0), Point(x, PlotArea.rect.height)])
				plot_canvas.pen(color=(0, 0, 0), width=0.5, dash=False)
				plot_canvas.draw_line([Point(x, 0), Point(x, -10)])

				if self.raw_labels:
					segment_label = "#{}".format(self.sample_number_from_value(value))
				else:
					segment_label = "{}".format(round(value, precision)+0)
				target_canvas.draw_text(BottomAxis.rect.p1 + Point(x, 10), segment_label, size=16, anchor=(0, -1))


			# determine plot boundaries
			value_start = self.value_from_y(0)
			value_end   = self.value_from_y(PlotArea.rect.height)

			# minimal vertical distance between two horizontal grid lines
			dy_min = 35

			while True:
				# minimal value step between two horizontal grid lines
				dvalue_min  = self.value_from_y(dy_min) - value_start
				if dvalue_min > 0:
					break

				# if we zoomed in too much, draw lines less often
				dy_min = dy_min * 2

			# number of digits displayed in decimal representation of the values
			precision = -int(math.floor(math.log(dvalue_min, 10)))


			# find the best decimal step
			decimal_value_step = 10 ** -precision
			value_step = first(lambda x: x >= dvalue_min, [1*decimal_value_step, 2*decimal_value_step, 5*decimal_value_step, 10*decimal_value_step])
			for value in float_range(ceil_by_multiple(value_start, value_step ), value_end + value_step, value_step):
				if value < epsilon:
					value = 0

				y = self.y_from_value(value)

				if y < 0 or y > PlotArea.rect.height:
					continue

				plot_canvas.pen(color=(0, 0, 0), width=0.5, dash=True)
				plot_canvas.draw_line([Point(0, y), Point(PlotArea.rect.width, y)])
				plot_canvas.pen(color=(0, 0, 0), width=0.5, dash=False)
				plot_canvas.draw_line([Point(0, y), Point(-10, y)])

				if self.raw_labels:
					segment_label = "{:04X}".format(self.raw_from_value(value))
				else:
					segment_label = int(round(value, precision)*1000)

				target_canvas.draw_text(LeftAxis.rect.p2 - Point(10, y), segment_label, size=16, anchor=(1, 0))

			target_canvas.draw_text(BottomAxis.rect.p1 + Point(BottomAxis.rect.width/2, 48), "seconds", size=16, anchor=(0, -1))
			target_canvas.draw_text(  LeftAxis.rect.p1 + Point(0, LeftAxis.rect.height/2), "mA", size=16, anchor=(0, 0))
		except ZeroDivisionError:
			pass # some values were missing to calculate, nothing can be done


		plot_canvas.pen(dash=False)

		plot_canvas.pen(color=(0, 0, 0), width=2)
		plot_canvas.draw_line([Point(0, 0), Point(0, PlotArea.rect.height), Point(PlotArea.rect.width, PlotArea.rect.height), Point(PlotArea.rect.width, 0), Point(0, 0)])

		plot_canvas.pen(color=(255, 0, 0), width=1)
		plot_canvas.draw_line(points(1))

		target_canvas.end()

