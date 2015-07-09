HDRDIR=hdr
SRCDIR=src
OBJDIR=obj
BINDIR=bin

CC=cc
CPP=c++
MKDIR=mkdir -p
RM=rm -r -f

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
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_posix.o
LIBS=
OSINT+=gpio_sysfs.o
endif

ifeq ($(OS),msys)
CFLAGS+=-DMINGW
EXT=.exe
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_mingw.o
LIBS=-lsetupapi
endif

ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=
OSINT=$(OBJDIR)/sock_posix.o $(OBJDIR)/serial_posix.o
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

all:	 $(BINDIR)/proploader$(EXT) blink.binary blink-slow.binary toggle.binary

$(OBJS):	$(OBJDIR) $(HDRS) Makefile

$(BINDIR)/proploader$(EXT):	$(BINDIR) $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LIBS) -lstdc++

%.binary:	%.elf
	propeller-load -s $<

%.elf:	%.c
	propeller-elf-gcc -Os -mlmm -o $@ $<
    
%.binary:	%.spin
	openspin $<

%-slow.binary:	%.spin
	openspin -DSLOW -o $@ $<

setup:	blink-slow.binary
	propeller-load -e blink-slow.binary

run:	$(BINDIR)/proploader$(EXT) blink.binary
	$(BINDIR)/proploader$(EXT) blink.binary

runbig:	$(BINDIR)/proploader$(EXT) toggle.binary
	$(BINDIR)/proploader$(EXT) toggle.binary

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(OBJDIR) $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp $(OBJDIR) $(HDRS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(DIRS):
	$(MKDIR) $@

clean:
	$(RM) $(OBJDIR) $(BINDIR) *.binary *.elf
