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

1) Add retry logic to get/set item as well as the loader code.

2) Switch to Jeff's new download protocol.

3) Create a program that will process the binary resulting from compiling IP_loader.spin.

The MIT License (MIT)

Copyright (c) 2015 David Michael Betz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
