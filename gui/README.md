# PowerGraph GUI
GUI for the PowerGraph measurement platform.

When the user space and kernel space modules are up and running on a Raspberry Pi, this GUI can be used to connect to the Pi to configure the PowerGraph board and to start measurements.
Please note that the Pi running the user space module announces itself via a periodic UDP broadcast. Therefore it is often not necessary to find the IP number of the Raspberry Pi to connect to it. If your network does not block UDP broadcasts, a connection can be established automatically from inside the GUI.

The GUI was testet with OSX 10.10.4 and Python 3.4.

Run `python3.4 src/main.py` from the gui folder. With a click on `Connect`, the GUI starts discovering the Pi with the PowerGraph Board (if the respective modules are installed and running on the Pi). A sample record is provided, which can be opened by clicking `Quick Open` and selecting `sample`. To change the scale of the x and y axis, you can zoom in and out by using your scroll wheel while hovering the mouse over the respective axis.  