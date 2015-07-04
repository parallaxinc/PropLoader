HDRDIR=hdr
SRCDIR=src
OBJDIR=obj
BINDIR=bin

CC=cc
CPP=c++
MKDIR=mkdir -p
RM=rm -r -f

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
$(OBJDIR)/sock_posix.o \
$(OBJDIR)/serial_posix.o

DIRS=$(OBJDIR) $(BINDIR)

CFLAGS=-DMACOSX -I$(HDRDIR)
CPPFLAGS=$(CFLAGS)

all:	 $(BINDIR)/proploader 

$(OBJS):	$(OBJDIR) $(HDRS) Makefile

$(BINDIR)/proploader:	$(BINDIR) $(OBJS)
	$(CPP) -o $@ $(OBJS) -lstdc++

blink.binary:	blink.spin
	openspin $<

run:	$(BINDIR)/proploader blink.binary
	$(BINDIR)/proploader blink.binary

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(OBJDIR) $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp $(OBJDIR) $(HDRS)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(DIRS):
	$(MKDIR) $@

clean:
	$(RM) $(OBJDIR) $(BINDIR) blink.binary
