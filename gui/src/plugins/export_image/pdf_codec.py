
from tkinter import filedialog


helvetica_widths = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	278, 278,355, 556, 556, 889, 667, 222, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556,
	556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556, 102, 667, 667, 722, 722, 667, 611, 778, 722, 278,
	500, 667, 556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469,
	556, 222, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500,
	278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 333, 556, 556, 167, 556, 556, 556, 556, 191, 333, 556, 333, 333, 500, 500, 0, 556, 556, 556, 278, 0,
	537, 350, 222, 333, 333, 556, 100, 100, 0, 611, 0, 333, 333, 333, 333, 333, 333, 333, 333, 0, 333, 333,
	333, 333, 333, 100,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 370, 0, 0, 0, 0, 0, 556, 778, 100, 365, 0, 0, 0,
	0, 0, 889, 0, 0, 0, 278, 0, 0, 222, 611, 944, 611, 0, 0, 0, 0]

def calc_text_width(text):
	width = 0
	for c in text:
		width += helvetica_widths[ord(c)]
	return width / 1000.0


class PdfString:
	def __init__(self, value):
		self.value = value

class PdfReference:
	def __init__(self, index, obj):
		self.index = index
		self.obj = obj
	def __str__(self):
		return "{} 0 R".format(self.index)

class PdfStream:
	attributes = dict()
	stream = ''
	def __init__(self, **kwargs):
		self.attributes = kwargs
		self.stream = ''
	def append(self, data):
		self.stream += data
	def append_line(self, data):
		self.stream += data
		self.stream += '\x0A'

def pdf_object_str(obj):
	t = type(obj)
	if obj is None:
		return "null"
	if t == bool:
		return "true" if obj else "false"
	if t == int:
		return str(obj)
	if t == list:
		return "[ {} ]".format(" ".join(map(lambda e: pdf_object_str(e), obj)))
	if t == str:
		return "/{}".format(obj)
	if t == PdfString:
		return "({})".format(obj.value)
	if t == dict:
		s = []
		for key, element in obj.items():
			s.append("/{} {}".format(key, pdf_object_str(element)))
		return "<< {} >>".format(" ".join(s))
	if t == PdfReference:
		return str(obj)
	if t == PdfStream:
		attr = obj.attributes
		encoded = obj.stream
		encoded.rstrip('\x0A')
		attr['Length'] = len(encoded)
		#attr['Filter'] = 'ASCIIHexDecode'
		#encoded = ''.join('{:02X}'.format(x) for x in obj.stream.encode()) + ">"
		return "{} stream\x0A{}\x0Aendstream".format(pdf_object_str(attr), encoded)
	raise TypeError("unsupported PDF object type: {}".format(type(obj).__name__))


class PdfObjectFormat:
	s = None

	object_contents = None

	def __init__(self, output_stream):
		self.s = output_stream
		self.object_contents = []

	def begin(self):
		self.s.write(bytes.fromhex('255044462D312E340A25ACDC20ABBA0A')) # magic string

	def put_line(self, line):
		if type(line) == str:
			self.s.write(line.encode())
		else:
			self.s.write(line)
		self.s.write(b'\x0A')

	def put_object(self, obj):
		# store the object to print it later
		self.object_contents.append(obj)

		# appended item will have this index
		obj_index = len(self.object_contents)

		# the return value can be used as indirect reference to the object
		return PdfReference(obj_index, obj)

	def end(self, root):
		object_locations = []
		# generate object contents
		for i, obj in enumerate(self.object_contents):
			object_index = i + 1

			# append the object location to our list of locations
			object_locations.append(self.s.tell())

			# output the object string representation, wrapped by indirect reference
			self.put_line("{} 0 obj".format(object_index))
			self.put_line(pdf_object_str(obj))
			self.put_line("endobj")

		info_obj = self.put_object(dict(CreationDate=PdfString("D:20150220140517+02'00'"), Producer=PdfString("PowerGraph pdf plugin"), Creator=PdfString("PowerGraph")) )
		xref_pos = self.s.tell()
		self.put_line("xref")
		self.put_line("0 {}".format(len(self.object_contents)))
		self.put_line("0000000000 65535 f")
		for position in object_locations:
			self.put_line("{:010} {:05} n".format(position, 0))
		self.put_line("trailer")
		self.put_line("<< /Info {} /Root {} /Size {} >>".format(info_obj, root, len(self.object_contents)))
		self.put_line("startxref")
		self.put_line(str(xref_pos))
		self.put_line("%%EOF")

		self.s.close()


class PdfVectorGraphic:
	o = None
	contents = None
	size = (0, 0)

	def __init__(self, output_stream):
		self.o = PdfObjectFormat(output_stream)
		self.contents = []

	def cmd(self, s):
		self.contents.append(s)
		self.contents.append("\x0A")

	def begin(self, size):
		self.size = size
		self.o.begin()

	def put_text(self, pos, text, size=12, opacity=1, outline_width=None):
		self.cmd("BT")                                # begin text
		self.cmd("{} {} Td".format(pos[0], pos[1]))   # position
		self.cmd("/F1 {} Tf".format(size))            # font
		if opacity < 1:
			self.cmd("{} g".format(1-opacity))          # gray transparancy
		else:
			self.cmd("0 g")                           # opaque
		if outline_width is not None:
			self.cmd("{} Tr".format(1))               # render mode
			self.cmd("{} w".format(outline_width))    # pen width
		self.cmd("({}) Tj".format(text))              # put text
		self.cmd("ET")                                # end text

	def put_line(self, points, color=(0, 0, 0), width=1, dash=False):
		self.cmd("{} {} {} RG".format(color[0], color[1], color[2]))              # pen color
		if dash:
			self.cmd("[2 4] 0 d")                         # pen dash
		else:
			self.cmd("[1 0] 0 d")                         # pen dash
		self.cmd("{} w".format(width))                    # pen width
		points = list(points)
		if len(points) < 2:
			return
		p = points[0]
		self.cmd("{} {} m".format(p[0], p[1]))        # move to
		for p in points[1:]:
			self.cmd("{} {} l".format(p[0], p[1]))    # line to
		self.cmd("S")                                 # draw line

	def put_polygon(self, points, color, width=1, dash=False):
		self.cmd("{} {} {} rg".format(color[0], color[1], color[2]))              # pen color
		if dash:
			self.cmd("[4 4] 0 d")                         # pen dash
		else:
			self.cmd("[1 0] 0 d")                         # pen dash
		self.cmd("{} w".format(width))                    # pen width
		points = list(points)
		if len(points) < 2:
			return
		p = points[0]
		self.cmd("{} {} m".format(p[0], p[1]))        # move to
		for p in points[1:]:
			self.cmd("{} {} l".format(p[0], p[1]))    # line to
		self.cmd("f")                                 # draw polygon

	def end(self):
		o = self.o.put_object
		o_contents = o(PdfStream())
		o_contents.obj.stream = ''.join(self.contents)
		main_font  = dict(F1=dict(Type='Font', Subtype='Type1', BaseFont='Helvetica'), F2=dict(Type='Font', Subtype='Type1', BaseFont='Courier'))
		o_page     = o(dict(Type='Page', MediaBox=[0, 0, self.size[0], self.size[1]], Contents=o_contents, Resources=dict(Font=main_font)))
		o_pages    = o(dict(Count=1, Kids=[o_page], Type='Pages'))
		o_root     = o(dict(Type='Catalog', Pages=o_pages))
		o_page.obj['Parent'] = o_pages
		self.o.end(o_root)


class PdfCanvas:
	''' all drawing commands will be redirected into the Tkinter Canvas widget '''
	pdf_target = 0
	pdf_pen_color = (0, 0, 0)
	pdf_pen_width = 1
	pdf_pen_dash = False

	size = (0, 0)
	def __init__(self, pdf_target):
		self.pdf_target = pdf_target
	
	def begin(self, size):
		self.size = size
		self.pdf_target.begin(size)
	def end(self):
		self.pdf_target.end()

	def _transform_point(self, p):
		return (p[0], self.size[1] - p[1])

	def pen(self, color=None, width=None, dash=None):
		if color is not None:
			r, g, b = color
			self.pdf_pen_color = (r/255, g/255, b/255)

		if width is not None:
			self.pdf_pen_width = width

		if dash is not None:
			self.pdf_pen_dash = True if dash else False

	def draw_line(self, list_of_points):
		self.pdf_target.put_line(map(self._transform_point, list_of_points), color=self.pdf_pen_color, width=self.pdf_pen_width, dash=self.pdf_pen_dash)

	def draw_polygon(self, list_of_points):
		self.pdf_target.put_polygon(map(self._transform_point, list_of_points), color=self.pdf_pen_color, width=self.pdf_pen_width, dash=self.pdf_pen_dash)

	def draw_text(self, pos, text, size=12, anchor=(-1, -1)):
		text = str(text)
		pos = self._transform_point(pos)

		# PDF sizes are smaller than tkinters
		size += 4

		# PDF does not support anchoring/alignment, simulate this feature manually
		# in PDF, all text is always anchored to left-bottom
		# subtract the text width accordingly
		xmove = (anchor[0]+1)/2
		ymove = (1-anchor[1])/2
		pos = (pos[0] - calc_text_width(text)*size*xmove,   pos[1] - size*ymove)

		self.pdf_target.put_text(pos, text, size)



class ExporterCanvas(PdfCanvas):
	def __init__(self, parent_tkinter_widget, default_filename, default_folder):
		# ask user for a save location
		filename = filedialog.asksaveasfilename(parent=parent_tkinter_widget, title="Export...", filetypes=[('PDF files', ".pdf")], initialfile=default_filename, initialdir=default_folder)
		if filename == "":
			raise Exception("user cancelled")
		if not filename.endswith(".pdf"):
			filename += ".pdf"
		PdfCanvas.__init__(self, PdfVectorGraphic(open(filename, 'wb')))

if __name__ == '__main__':
	f = open("test.pdf", 'wb')
	pdf = PdfVectorGraphic(f)

	pdf.begin((500, 400))
	pdf.put_text((10, 10), "test")
	pdf.put_line([(0, 0), (100, 100)], color=(1, 0, 0))
	pdf.end()

