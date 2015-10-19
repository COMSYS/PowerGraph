import GUI
from folder import Folder
import os
import io


# popup a dialog that lists all the files in the /records/ folder
def ask_choose_record(parent, spawn_coords):
		# entries of listbox are the list of files
		entry_list = enumerate_records()

		# init popup dialog
		window = GUI.Toplevel(parent, takefocus=True)
		window.wm_title("/record/ folder contents")
		window.transient(parent)
		window.resizable(0,0)
		window.minsize(100, 100 + 20*len(entry_list))

		# add some text
		label = GUI.Label(window, text="choose:")
		label.pack(fill='both')

		# init listbox
		listbox = GUI.Listbox(window)
		listbox.pack(fill='both', expand=1, padx=2, pady=2)

		listbox.delete(0, GUI.END)
		for entry in entry_list:
			listbox.insert(GUI.END, entry)

		def on_select(e):
			def callback():
				on_select.select_index = listbox.curselection()
				window.destroy()
				window.quit()
			parent.after(100, callback)
		listbox.bind('<<ListboxSelect>>', on_select)

		# put window in front of others (popup-action)
		window.focus_set()
		window.bind("<FocusOut>", lambda e: window.destroy())

		# center window into the center of the parent window
		window.geometry('%dx%d+%d+%d' % ((window.winfo_reqwidth(), window.winfo_reqheight()) + spawn_coords)) # set dimensions of window
		window.mainloop()

		i = on_select.select_index
		if type(i) != tuple:
			return None
		return entry_list[int(i[0])]




try:
	record_folder = Folder(os.path.dirname(os.path.realpath(__file__)) + "/../records")
except:
	print("could not find /records/ directory")
	exit() # fatal error



# if file does not exist, raises FileNotFoundError
def open_record_file(record_name, for_writing=False):

	if not record_name.endswith(".pgrecord"):
		record_name += ".pgrecord"

	f = open(record_folder.file_path(record_name), 'wb' if for_writing else 'rb')

	if not for_writing:
		f = io.BufferedReader(f, buffer_size=1024*1024)
	
	return f


# return a list of all filenames in the /records/ folder, without the ".pgrecord" extension
def enumerate_records():
	file_names = map(lambda fn: fn[:-9], filter(lambda fn: fn.endswith(".pgrecord"), record_folder.list_file_names()))
	return sorted( file_names, key=lambda x: (len(x), x) )

