MKDIR=mkdir
TOUCH=touch
RM=rm -r -f

ifeq ($(CROSS),)
  PREFIX=
else
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

CC=$(PREFIX)gcc
CPP=$(PREFIX)g++

CFLAGS=-Wall

ifeq ($(OS),Windows_NT)
OS=msys
endif

ifeq ($(OS),linux)
CFLAGS+=-DLINUX
EXT=
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_posix.o
LIBS=

else ifeq ($(OS),raspberrypi)
OS=linux
CFLAGS+=-DLINUX -DRASPBERRY_PI
EXT=
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_posix.o $(OBJDIR)/gpio_sysfs.o
LIBS=

else ifeq ($(OS),msys)
CFLAGS+=-DMINGW
EXT=.exe
OSINT=$(OBJDIR)/serial_mingw.o $(OBJDIR)/sock_posix.o $(OBJDIR)/enumcom.o
LIBS=-lws2_32 -liphlpapi -lsetupapi

else ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=
OSINT=$(OBJDIR)/serial_posix.o $(OBJDIR)/sock_posix.o
LIBS=

else ifeq ($(OS),)
$(error OS not set)

else
$(error Unknown OS $(OS))
endif

BUILD=$(realpath ..)/proploader-$(OS)-build

HDRDIR=hdr
SRCDIR=src
OBJDIR=$(BUILD)/obj
BINDIR=$(BUILD)/bin

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
$(OBJDIR)/loadelf.o \
$(OSINT)

CFLAGS+=-I$(HDRDIR)
CPPFLAGS=$(CFLAGS)

all:	 $(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary $(BUILD)/blink-slow.binary $(BUILD)/toggle.elf

$(OBJS):	$(OBJDIR)/created $(HDRS) Makefile

$(BINDIR)/proploader$(EXT):	$(BINDIR)/created $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LIBS) -lstdc++

$(BUILD)/%.elf:	%.c
	propeller-elf-gcc -Os -mlmm -o $@ $<
    
$(BUILD)/%.binary:	%.spin
	openspin -o $@ $<

$(BUILD)/%-slow.binary:	%.spin
	openspin -DSLOW -o $@ $<

setup:	$(BUILD)/blink-slow.binary
	propeller-load -e $(BUILD)/blink-slow.binary

run:	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary -t

runbig:	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf
	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf -t

E:	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/blink.binary -e

Ebig:	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf
	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf -e

P:	$(BINDIR)/proploader$(EXT)
	$(BINDIR)/proploader$(EXT) -P
	
P0:	$(BINDIR)/proploader$(EXT)
	$(BINDIR)/proploader$(EXT) -P0
	
X:	$(BINDIR)/proploader$(EXT)
	$(BINDIR)/proploader$(EXT) -X
	
X0:	$(BINDIR)/proploader$(EXT)
	$(BINDIR)/proploader$(EXT) -X0
	
$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp $(HDRS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(BUILD)

%/created:
	@$(MKDIR) -p $(@D)
	@$(TOUCH) $@
