# User Space Program

This folder contains the user space program to interface with PowerGraph. You can compile it on the Raspberry pi using the supplied *Makefile* to the binary *host*. 
This program monitors the physical button on the PowerGraph hardware and performs the measurement by streaming the data into a "records" folder on the Raspberry Pi. This program also acts as a TCP server to interface with the GUI running on a remote PC. Measurements can be started/stopped remotely from the GUI.

The server announces its presence in the local network via an UDP broadcast. This allows to capture the IP address of the PI from within the GUI, automatically, provided the network does not block UDP broadcasts.
