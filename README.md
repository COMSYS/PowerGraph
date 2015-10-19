# PowerGraph
Driver and GUI for the PowerGraph measurement platform

PowerGraph is split into kernel- and user space part and a GUI running on a remote machine.

To run PowerGraph, just compile the kernel module and load it on the Raspberry pi.
Subsequently, compile the user space program and start it up. Now you can use the GUI to connect, configure, and use PowerGraph from a remote machine.

The quick open button displays the files that lie in the "records" folder. When downloading new records from the Raspberry Pi, these will be put into the "records" folder as well.

More information about the hardware and the project can be found [here](https://www.comsys.rwth-aachen.de/short/powergraph).
