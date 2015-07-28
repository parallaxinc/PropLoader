MKDIR=mkdir -p
RM=rm -r -f

ifeq ($(CROSS),)
  BUILD=$(realpath ..)/proploader-build
  PREFIX=
else
  BUILD=$(realpath ..)/proploader-$(CROSS)-build
  ifeq ($(CROSS),win32)
    PREFIX=i586-mingw32msvc-
    OS=msys
  else
    ifeq ($(CROSS),rpi)
      PREFIX=arm-linux-gnueabihf-
      OS=linux
    else
      echo "Unknown cross compilation selected"
    endif
  endif
endif

HDRDIR=hdr
SRCDIR=src
OBJDIR=$(BUILD)/obj
BINDIR=$(BUILD)/bin

CC=$(PREFIX)gcc
CPP=$(PREFIX)g++

CFLAGS=-Wall

ifeq ($(OS),linux)
CFLAGS+=-DLINUX
EXT=
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_posix.o
LIBS=
endif

ifeq ($(OS),raspberrypi)
OS=linux
CFLAGS+=-DLINUX -DRASPBERRY_PI
EXT=
OSINT=$(OBJDIR)/serial_posix.o $(OBJDIR)/sock_posix.o
LIBS=
OSINT+=gpio_sysfs.o
endif

ifeq ($(OS),msys)
CFLAGS+=-DMINGW
EXT=.exe
OSINT=$(OBJDIR)/serial_mingw.o $(OBJDIR)/sock_posix.o
LIBS=-lws2_32 -liphlpapi
endif

ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=
OSINT=$(OBJDIR)/serial_posix.o $(OBJDIR)/sock_posix.o
LIBS=
endif

ifeq ($(OS),)
$(error OS not set)
endif

HDRS=\
$(HDRDIR)/loader.hpp \
$(HDRDIR)/serial-loader.hpp \
$(HDRDIR)/xbee-loader.hpp \
$(HDRDIR)/xbee.hpp \
$(HDRDIR)/sock.h \
$(HDRDIR)/serial.h

OBJS=\
$(OBJDIR)/main.o \
$(OBJDIR)/loader.o \
$(OBJDIR)/xbee-loader.o \
$(OBJDIR)/serial-loader.o \
$(OBJDIR)/xbee.o \
$(OSINT)

CFLAGS+=-I$(HDRDIR)
CPPFLAGS=$(CFLAGS)

DIRS=$(OBJDIR) $(BINDIR)

all:	 $(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary $(BUILD)/blink-slow.binary $(BUILD)/toggle.binary

$(OBJS):	$(OBJDIR) $(HDRS) Makefile

$(BINDIR)/proploader$(EXT):	$(BINDIR) $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LIBS) -lstdc++

$(BUILD)/%.binary:	$(BUILD)/%.elf
	propeller-load -s $<

$(BUILD)/%.elf:	%.c
	propeller-elf-gcc -Os -mlmm -o $@ $<
    
$(BUILD)/%.binary:	%.spin
	openspin -o $@ $<

$(BUILD)/%-slow.binary:	%.spin
	openspin -DSLOW -o $@ $<

setup:	$(BUILD)/blink-slow.binary
	propeller-load -e $(BUILD)/blink-slow.binary

run:	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary

runbig:	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.binary

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(OBJDIR) $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp $(OBJDIR) $(HDRS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(DIRS):
	$(MKDIR) $@

clean:
	$(RM) $(BUILD)
