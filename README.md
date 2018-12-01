This is a loader for the Parallax Propeller

It supports loading over a serial link or a WiFi connection to a Parallax WiFi module on the
Propeller Activity Board WX.

```
PropLoader v1.0-43 (2018-12-01 13:55:21 g7445ae2)

usage: proploader [options] [<file>]

options:
    -b <type>       select target board and subtype (default is 'default:default')
    -c              display numeric message codes
    -D var=value    define a board configuration variable
    -e              program eeprom (and halt, unless combined with -r)
    -f <file>       write a file to the SD card
    -i <ip-addr>    IP address of the Parallax Wi-Fi module
    -I <path>       add a directory to the include path
    -n <name>       set the name of a Parallax Wi-Fi module
    -p <port>       serial port
    -P              show all serial ports
    -r              run program after downloading (useful with -e)
    -R              reset the Propeller
    -s              do a serial download
    -t              enter terminal mode after the load is complete
    -T              enter pst-compatible terminal mode after the load is complete
    -v              enable verbose debugging output
    -W              show all discovered wifi modules
    -?              display a usage message and exit

file:               binary file to load (.elf or .binary)

Target board type can be either a single identifier like 'propboe' in which case the subtype
defaults to 'default' or it can be of the form <type>:<subtype> like 'c3:ram'.

Module names should only include the characters A-Z, a-z, 0-9, or '-' and should not begin or
end with a '-'. They must also be less than 32 characters long.

Variables that can be set with -D are:

Used by the loader:
  loader reset clkfreq clkmode fast-loader-clkfreq fastloader-clkmode
  baudrate loader-baud-rate fast-loader-baud-rate

Used by the SD file writer:
  sdspi-do sdspi-clk sdspi-di sdspi-cs
  sdspi-clr sdspi-inc sdspi-start sdspi-width spdspi-addr
  sdspi-config1 sdspi-config2

Value expressions for -D can include:
  rcfast rcslow xinput xtal1 xtal2 xtal3 pll1x pll2x pll4x pll8x pll16x k m mhz true false
  an integer or two operands with a binary operator + - * / % & | or unary + or -
  or a parenthesized expression.

Examples:
  loader=rom  to use the ROM loader instead of the fast loader
```

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
