import math
import itertools


class Point:
	''' Represent a 2D vector. '''
	def __init__(self, *args):

		if len(args) == 0:
			self.x = 0
			self.y = 0

		# one argument is a conversion
		if len(args) == 1:
			try:
				self.x = args[0][0]
				self.y = args[0][1]
			except TypeError:
				raise TypeError("solo parameter of Point() must have x[0] and x[1]")
		
		# two arguments are x and y
		if len(args) == 2:
			self.x = args[0]
			self.y = args[1]

	def __eq__(self, other):
		return (self.x == other.x) and (self.y == other.y)

	def __abs__(self):
		return math.sqrt(self.x*self.x + self.y*self.y)

	def __add__(self, val):
		return Point( self.x + val.x, self.y + val.y )

	def __sub__(self,val):
		return Point( self.x - val.x, self.y - val.y )

	def __iadd__(self, val):
		self.x += val.x
		self.y += val.y
		return self

	def __isub__(self, val):
		self.x -= val.x
		self.y -= val.y
		return self

	def __div__(self, val):
		return Point( self.x / val, self.y / val )

	def __mul__(self, val):
		return Point( self.x * val, self.y * val )

	def __idiv__(self, val):
		self.x /= val
		self.y /= val
		return self

	def __imul__(self, val):
		self.x *= val
		self.y *= val
		return self
		
	def __getitem__(self, key):
		if key == 0:
			return self.x
		elif key == 1:
			return self.y
		else:
			raise Exception("Invalid key to Point, must be 0 or 1")

	def __setitem__(self, key, value):
		if key == 0:
			self.x = value
		elif key == 1:
			self.y = value
		else:
			raise Exception("Invalid key to Point, must be 0 or 1")

	def __str__(self):
		return "Point({}, {})".format(self.x, self.y)

	def __iter__(self):
		return iter((self.x, self.y))

	def scale_by(self, scale_point):
		return Point(self.x * scale_point.x, self.y * scale_point.y)

	def snap(self):
		return Point(round(self.x), round(self.y))


class Rectangle:
	# each parameter is a Point()
	def __init__(self, p1, p2=None, size=None):
		self.p1 = p1
		if p2 is not None:
			self.p2 = p2
		if size is not None:
			self.p2 = p1+size

	@property
	def size(self):
		return self.p2 - self.p1   # returns Point()

	@size.setter
	def size(self, value):
		self.p2 = self.p1 + value

	# return True if the point is inside the rectangle
	def is_inside(self, point):
		return ((point.x >= self.p1.x) and (point.y >= self.p1.y) and (point.x < self.p2.x) and (point.y < self.p2.y))

	# transform a point to the rectangle's local coordinates
	def global_to_local(self, point):
		return point - self.p1

	# returns the closest point inside the rectangle
	def limit(self, point):
		retval = Point(point)
		if retval.x < self.p1.x:
			retval.x = self.p1.x
		if retval.y < self.p1.y:
			retval.y = self.p1.y
		if retval.x > self.p2.x:
			retval.x = self.p2.x
		if retval.y > self.p2.y:
			retval.y = self.p2.y
		return retval

	@property
	def width(self):
		return self.size.x
	@property
	def height(self):
		return self.size.y
	@property
	def left(self):
		return self.p1.x
	@property
	def top(self):
		return self.p1.y
	@property
	def right(self):
		return self.p2.x
	@property
	def bottom(self):
		return self.p2.y

class CanvasTransform:
	''' Wraps another Canvas, exposes a new Canvas interface. All drawing commands will be transformed. '''
	target_canvas = 0
	offset_point = Point(0, 0)
	scale_point = Point(1, 1)
	def __init__(self, target_canvas, offset=None, scale=None):
		self.target_canvas = target_canvas
		if offset is not None:
			self.offset_point = offset
		if scale is not None:
			self.scale_point = scale
	
	# override this function if you need more complicated transform
	def transform_point(self, p):
		if not isinstance(p, Point):
			raise TypeError("CanvasTransform can only transform Points")
		return p.scale_by(self.scale_point) + self.offset_point
	
	def inverse_transform_point(self, p):
		pp = p - offset_point
		return Point(pp.x / scale_point.x, pp.y / scale_point.y)

	# graphical primitives functions
	def draw_line(self, list_of_points):
		self.target_canvas.draw_line(self.transform_point(p) for p in list_of_points)

	def draw_polygon(self, list_of_points):
		self.target_canvas.draw_polygon(self.transform_point(p) for p in list_of_points)

	def draw_text(self, pos, text, **kwargs):
		self.target_canvas.draw_text(self.transform_point(pos), text, **kwargs)

	def pen(self, *args, **kwargs):
		self.target_canvas.pen(*args, **kwargs)


# convert a color (r, g, b) into the representation #RRGGBB
def make_tkinter_color(color):
	return "#{:0>2X}{:0>2X}{:0>2X}".format(color[0], color[1], color[2])

# convert an anchor direction like (0, 0) into a format required by tkinter, like "nw"
def make_tkinter_anchor(anchor):
	a = ''
	if anchor[1] < 0:
		a += 'n'
	if anchor[1] > 0:
		a += 's'
	if anchor[0] < 0:
		a += 'w'
	if anchor[0] > 0:
		a += 'e'
	if a == '':
		a = 'center'
	return a


class CanvasTkinterAdapter:
	''' all drawing commands will be redirected into the Tkinter Canvas widget '''
	tkinter_canvas = 0
	tkinter_pen_color = '#000'
	tkinter_pen_width = 1
	tkinter_pen_dash = None
	def __init__(self, tkinter_canvas):
		self.tkinter_canvas = tkinter_canvas

	def begin(self, size):
		pass
	def end(self):
		pass
	
	def pen(self, color=None, width=None, dash=None):
		if color is not None:
			self.tkinter_pen_color = make_tkinter_color(color)

		if width is not None:
			self.tkinter_pen_width = width

		if dash is not None:
			self.tkinter_pen_dash = True if dash else None

	def draw_line(self, list_of_points):
		try:
			x_y_chain = itertools.chain.from_iterable((p.x, p.y) for p in list_of_points)
			self.tkinter_canvas.create_line(*x_y_chain, fill=self.tkinter_pen_color, width=self.tkinter_pen_width, dash=self.tkinter_pen_dash)
		except IndexError:
			pass

	def draw_polygon(self, list_of_points):
		try:
			x_y_chain = itertools.chain.from_iterable((p.x, p.y) for p in list_of_points)
			self.tkinter_canvas.create_polygon(*x_y_chain, fill=self.tkinter_pen_color, width=self.tkinter_pen_width)
		except IndexError:
			pass

	def draw_text(self, pos, text, size=12, anchor=(-1, -1)):
		self.tkinter_canvas.create_text(tuple(pos), text=text, fill=self.tkinter_pen_color, font=("Helvetica", size), anchor=make_tkinter_anchor(anchor))









if __name__ == '__main__':
	assert( Point((2, 2)) + Point(3, 3) == Point(1, 1)*5 )
	print( Point(2, 1) )
