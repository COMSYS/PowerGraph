
import os


class Folder:
	''' hides the cryptic Python os interface, exposes a clean Pythonic interface '''

	def __init__(self, relative_path):
		self.path = os.path.abspath(relative_path)
		if not os.path.exists(self.path):
			raise ValueError("{} is not an existing folder".format(relative_path))

	def list_file_names(self):
		return os.listdir(self.path)

	def file_path(self, file_name):
		return self.path + "/" + file_name

	def file_exists(self, file_name):
		return os.path.exists(self.file_path(file_name))

