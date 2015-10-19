
from tkinter import filedialog
import tkinter.simpledialog


# create_output_file is a function that returns a StreamEncoder
def start(create_output_file):
	# ask user for a save location
	filename = filedialog.asksaveasfilename(parent=parent_tkinter_widget, title="Import...", filetypes=[('CSV files', ".csv")])

	# ask user for a save location
	message = tkinter.simpledialog.askstring("Git Push", "commit message")
