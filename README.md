This is a loader for the Parallax Propeller

It supports loading over a serial link or a WiFi connection to an Xbee S6B module on the
Propeller Activity Board WX.

This program is still under construction and not ready for use at this time.

The C++ code should be mostly generic. The platform-specific code is in the C files
sock_posix.c and serial_posix.c. Those will have to be rewritten to work on a different
platform or under a different framework like Qt. If necessary, those interfaces could
also be C++. I left them as C for now because they matched my original code better.

The files sock.h and serial.h show the interfaces needed to support another platform.
I can easily provide a Windows version of these files to cover running the loader
under Linux, Mac, and Windows. The xxx_posix.c files support both Linux and the Mac.

TODO:

1) Find a way to determine the host IP address for the interface used to connect to an
   Xbee module whose IP address is given on the command line.
   
2) Make Xbee discovery more robust.

3) Add retry logic to get/set item as well as the loader code.

4) Add .elf file handling. (done)

5) Switch to Jeff's new download protocol.

6) Create a program that will process the binary resulting from compiling IP_loader.spin.
