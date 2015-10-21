# PowerGraph
Driver and GUI for the PowerGraph measurement platform

PowerGraph is split into kernel- and user space part and a GUI running on a remote machine.

To run PowerGraph, just compile the kernel module and load it on the Raspberry pi.
Subsequently, compile the user space program and start it up. Now you can use the GUI to connect, configure, and use PowerGraph from a remote machine.

The quick open button displays the files that lie in the "records" folder. When downloading new records from the Raspberry Pi, these will be put into the "records" folder as well.

More information about the hardware and the project can be found [here](https://www.comsys.rwth-aachen.de/short/powergraph).

Copyright (C) 2014-2015, Roman Bartaschewitsch, <roman.bartaschewitsch@rwth-aachen.de> <br>
Copyright (C) 2014-2015, Torsten Zimmermann, <zimmermann@comsys.rwth-aachen.de> <br>
Copyright (C) 2014-2015, Jan Rüth, <rueth@comsys.rwth-aachen.de> <br>
Copyright (C) 2014-2015, Chair of Computer Science 4, RWTH Aachen University,<br>http://www.comsys.rwth-aachen.de


#License

#####FREE SOFTWARE

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.

#####IF YOU NEED ANOTHER LICENSE

If you are planning to integrate PowerGraph into a commercial product, please contact us for licensing options via email at:

  zimmermann@comsys.rwth-aachen.de<br>
  rueth@comsys.rwth-aachen.de

#####IF YOU REALLY LIKE OUR SOFTWARE
Buy us a beer when the situation arises :-)


#####OTHER ISSUES
This software is currently a proof of concept. It needs some more work to make it generally useable for a non technical person.

Open issues include:
* Stability of the GUI

#####HOW TO USE THIS SOFTWARE
* Please have a look at the individual Readme files provided for each software component
