
from folder import Folder
import os


class PluginFolder:
	''' represents the plugins folder '''

	def __init__(self, pythonic_path):
		self.pythonic_path = pythonic_path
		self.folder = Folder(os.path.dirname(__file__) + "/" + self.pythonic_path.replace(".", "/"))

	# returns a list of plugin names
	def enumerate(self):
		return [ fname[:-3] for fname in self.folder.list_file_names() if fname.endswith(".py") ]

	# returns a module object representing a loaded plugin
	def load(self, plugin_name):
		if not self.folder.file_exists(plugin_name + ".py"):
			raise ImportError("module name \"{}\" not found".format(plugin_name))

		# create pythonic path
		prefix = self.pythonic_path
		if prefix != "":
			prefix += "."

		full_module_name = prefix + plugin_name

		# now no path is needed
		module_obj = __import__(full_module_name)

		# browse through the python module tree
		for next_dive in full_module_name.split(".")[1:]:
			module_obj = getattr(module_obj, next_dive)

		return module_obj



