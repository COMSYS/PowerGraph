import GUI



class ProgressDialog(GUI.Toplevel):
	''' use the "with" statement to enter a scope with an accompanying dialog with a progress bar. The dialog is closes when the leaving the scope. '''

	progress_bar = None
	parent = None

	p = 0

	def __init__(self, parent, describtion="Working..."):
		self.parent = parent

		# init popup dialog
		GUI.Toplevel.__init__(self, parent, takefocus=True)
		self.wm_title(describtion)
		self.transient(parent)
		self.resizable(0,0)

		# add some text
		label = GUI.Label(self, text="progress:")
		label.pack(fill='both', expand=1, padx=2, pady=2)

		# init progress bar
		self.progress_bar = GUI.Progressbar(self, orient='horizontal', length=300, mode='determinate')
		self.progress_bar.pack(fill='both', expand=1, padx=2, pady=2)

		# put window in front of others (popup-action)
		self.lift()
		self.grab_set()
		self.focus_set()
		self.set_maximum(100)
		self.set_progress(0)

		# center window into the center of the parent window
		GUI.center_window(self, parent)


	def set_maximum(self, m):
		if m < 2:
			m = 2
		self.progress_bar.configure(maximum=(m-1))

	def set_progress(self, p):
		self.p = p
		self.progress_bar.configure(value=p)
		self.update()

	def add_progress(self, add_p):
		self.set_progress(self.p + add_p)

	def close(self):
		self.grab_release()
		self.parent.focus_set()
		self.update()
		self.destroy()
		self.update()

	def __enter__(self):
		return self

	def __exit__(self, exception_type, exception_object, exception_traceback):
		self.close()

		# if tkinter dialog was closed, swallow the exception
		if exception_type == GUI.TclError:
			return True




if __name__ == '__main__':
	import time
	gui_window = GUI.Tk()
	def on_test_click(e):
		with ProgressDialog(gui_window) as progress_dialog:
			for i in range(100):
				time.sleep( 0.01 )
				progress_dialog.set_progress(i)

	label = GUI.Label(gui_window, text="text in the Label")
	label.pack(fill='both', expand=1, padx=2, pady=2)
	gui_window.bind('<Button-1>', on_test_click)    # left mouse button
	gui_window.mainloop()
