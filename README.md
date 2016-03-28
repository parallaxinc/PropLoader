This is a loader for the Parallax Propeller

It supports loading over a serial link or a WiFi connection to a Parallax WiFi module on the
Propeller Activity Board WX.

This program is still under construction and not ready for use at this time.

The C++ code should be mostly generic. The platform-specific code is in the C files
sock_posix.c and serial_posix.c. Those will have to be rewritten to work on a different
platform or under a different framework like Qt. If necessary, those interfaces could
also be C++. I left them as C for now because they matched my original code better.

The files sock.h and serial.h show the interfaces needed to support another platform.
I can easily provide a Windows version of these files to cover running the loader
under Linux, Mac, and Windows. The xxx_posix.c files support both Linux and the Mac.
