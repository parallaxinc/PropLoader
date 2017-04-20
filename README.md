This is a loader for the Parallax Propeller

It supports loading over a serial link or a WiFi connection to a Parallax WiFi module on the
Propeller Activity Board WX.

The C++ code should be mostly generic. The platform-specific code is in the C files
sock_posix.c and serial_posix.c. Those will have to be rewritten to work on a different
platform or under a different framework like Qt. If necessary, those interfaces could
also be C++. I left them as C for now because they matched my original code better.

The files sock.h and serial.h show the interfaces needed to support another platform.
I can easily provide a Windows version of these files to cover running the loader
under Linux, Mac, and Windows. The xxx_posix.c files support both Linux and the Mac.

In addition to a standard C++ toolset you also need to install OpenSpin and have it
in your path. 

    https://github.com/parallaxinc/OpenSpin

To build the Windows version under Linux you will need the MinGW toolchain installed.
Then type:

    make CROSS=win32

Output files are placed:

    Macintosh:	../proploader-macosx-build/bin
    Linux:	../proploader-linux-build/bin
    Windows:	../proploader-msys-build/bin

To build the C test programs you also need PropGCC installed an in your path.
