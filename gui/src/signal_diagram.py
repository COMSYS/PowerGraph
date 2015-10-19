

import GUI
import canvas

Point = canvas.Point



def draw_arrow(self, p):
	size = 5
	self.draw_line([p + Point(-size, size), p, p + Point(size, size), p, p + Point(0, size*3)])


class SignalDiagram(GUI.Canvas):
	''' Renders a graphical representation of the electical digital signal expected on the SPI port. '''

	live_data = 0

	def __init__(self, tk_parent, config):
		self.config = config
		GUI.Canvas.__init__(self, tk_parent)
		self.configure(borderwidth=1, relief='sunken', bg='white', width=560, height=10)
		self.redraw()

	def redraw(self):
		self.delete('all')

		diagram_offset = Point(100, 40)

		c = canvas.CanvasTransform(canvas.CanvasTkinterAdapter(self), diagram_offset)

		bit_width = 14    # pixels
		signal_height = 20
		control_y = 20
		clock_y = 60
		live_data_y = 100

		# draw grid for orientation
		c.pen(dash=True, color=(225, 225, 225))
		for i in range(33):
			x = i*bit_width
			c.draw_line([Point(x, -10), Point(x, live_data_y+10)])

		# draw control signal
		c.pen(dash=False, color=(0, 0, 0))
		data_word = self.config.SPI_output_pattern
		line = []
		for i in range(-1, 33):
			x = i*bit_width
			data_bit = 1 if data_word & (0x80000000 >> (i%32)) else 0
			y = control_y - data_bit*signal_height
			line += [Point(x, y), Point(x+bit_width, y)]
		c.draw_line(line)
	
		# draw clock
		c.pen(dash=False, color=(0, 0, 0))
		clock_invert = (self.config.SPI_clock_mode & 0x1) != 0
		clock_delay  = (self.config.SPI_clock_mode & 0x2) != 0
		inversion_transform = lambda s: signal_height * (1-s if clock_invert else s)
		delay_x = bit_width/2 if clock_delay else 0

		# start line outside of the diagram
		overdraw = bit_width
		line = [Point(-overdraw, clock_y - inversion_transform(0))]

		for i in range(32):
			signal_point = lambda dx, s: Point((i + dx)*bit_width + delay_x, clock_y - inversion_transform(s))
			line += [signal_point(0, 0), signal_point(0, 1), signal_point(0.5, 1), signal_point(0.5, 0)]
		# end line outside of diagram
		line += [Point(32*bit_width + overdraw, clock_y - inversion_transform(0))]
		c.draw_line(line)


		# draw live data signal
		c.pen(dash=False, color=(255, 0, 0))
		data_word = self.live_data
		line = []
		for i in range(32):
			x = i*bit_width
			data_bit = 1 if (data_word & 0x80000000) != 0 else 0
			data_word = 0xFFFFFFFF & (data_word << 1)
			y = live_data_y - data_bit*signal_height
			line += [Point(x, y), Point(x+bit_width, y)]
		c.draw_line(line)

		# draw labels
		#
		# bit numbering
		c.pen(color=(225, 225, 225))
		c.draw_text(Point( 0.5 * bit_width, -10), "31", anchor=(0, 1))
		c.draw_text(Point(31.5 * bit_width, -10), "0",  anchor=(0, 1))

		# draw signal labels
		nc = canvas.CanvasTkinterAdapter(self)  # normal coordinates
		dy = diagram_offset[1]
		nc.pen(color=(255, 0, 0))
		nc.draw_text(Point(10, live_data_y + dy), "SPI input", anchor=(-1, 1))
		nc.pen(color=(0, 0, 0))
		nc.draw_text(Point(10, control_y + dy),   "SPI output", anchor=(-1, 1))
		nc.draw_text(Point(10, clock_y + dy),     "SPI clock", anchor=(-1, 1))

		# annotations for data extraction
		data_y = live_data_y + 10
		c.pen(color=(0, 0, 0))
		digital_channel_arrow_pos = Point((31.5 - self.config.digital_bit_shift) * bit_width, data_y)
		draw_arrow(c, digital_channel_arrow_pos)
		c.draw_text(digital_channel_arrow_pos + Point(0, 20), "digital", anchor=(0, -1))
		c.draw_text(digital_channel_arrow_pos + Point(0, 35), "channel", anchor=(0, -1))

		word = self.config.ADC_max
		ADC_bit_count = 0
		while word > 0:
			word >>= 1
			ADC_bit_count += 1
		ADC_start_bit = self.config.bit_shift
		ADC_end_bit = ADC_start_bit + ADC_bit_count
		ADC_start_x = (32 - ADC_start_bit) * bit_width
		ADC_end_x   = (32 - ADC_end_bit  ) * bit_width
		bracket_size = 5
		c.draw_line([Point(ADC_start_x, data_y), Point(ADC_start_x, data_y+bracket_size), Point(ADC_end_x, data_y+bracket_size), Point(ADC_end_x, data_y)])
		ADC_text_pos = Point((ADC_start_x+ADC_end_x)/2, data_y+bracket_size)
		c.draw_text(ADC_text_pos, "ADC data", anchor=(0, -1))

		mask = (1 << ADC_bit_count) - 1
		c.draw_text(ADC_text_pos + Point(0, 15), "currently: {:04X}".format((self.live_data >> ADC_start_bit) & mask), anchor=(0, -1))

	
		nc.draw_text(Point(10, 210), "clock freq: {:,}Hz".format(self.config.frequency*32), anchor=(-1, -1))



if __name__ == '__main__':
	tk = GUI.Tk()
	SignalDiagram(tk).pack(fill='both', expand=1)
	tk.mainloop()

