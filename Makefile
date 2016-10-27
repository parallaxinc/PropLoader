# version is MAJOR.minor-commit (date hash)
# if there has not been a commit since the last tag, commit and hash are left out
# MAJOR is the major version number (change when protocol changes)
# minor is the minor version number
# commit is the number of commits since the last tag
# date is the date of the build
# hash is a unique substring of the hash of the last commit
GITDESC=$(shell git describe master)
GITDESC_WORDS=$(subst -, ,$(GITDESC))
GIT_VERSION=$(word 1,$(GITDESC_WORDS))
GIT_BUILD=$(word 2,$(GITDESC_WORDS))
GIT_HASH=$(word 3,$(GITDESC_WORDS))
ifeq ($(GIT_BUILD),)
  GIT_BUILD_SUFFIX=
  GIT_HASH_SUFFIX=
else
  GIT_BUILD_SUFFIX=-$(GIT_BUILD)
  GIT_HASH_SUFFIX=$(subst -, ,-$(GIT_HASH))
endif
VERSION=$(GIT_VERSION)$(GIT_BUILD_SUFFIX) ($(shell date "+%Y-%m-%d %H:%M:%S")$(GIT_HASH_SUFFIX))
$(info VERSION $(VERSION))

MKDIR=mkdir
TOUCH=touch
RM=rm -r -f

ifeq ($(CROSS),)
  PREFIX=
else
  ifeq ($(CROSS),win32)
	PREFIX=i686-w64-mingw32-
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
SPINCMP=openspin
TOOLCC=gcc

CFLAGS=-Wall -DVERSION=\""$(VERSION)"\"
LDFLAGS=

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
LDFLAGS=-static
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

SRCDIR=src
OBJDIR=$(BUILD)/obj
BINDIR=$(BUILD)/bin
SPINDIR=spin
TOOLDIR=tools

OBJS=\
$(OBJDIR)/main.o \
$(OBJDIR)/loader.o \
$(OBJDIR)/fastloader.o \
$(OBJDIR)/propimage.o \
$(OBJDIR)/packet.o \
$(OBJDIR)/serialpropconnection.o \
$(OBJDIR)/serialloader.o \
$(OBJDIR)/wifipropconnection.o \
$(OBJDIR)/loadelf.o \
$(OBJDIR)/sd_helper.o \
$(OBJDIR)/config.o \
$(OBJDIR)/expr.o \
$(OBJDIR)/system.o \
$(OSINT)

CFLAGS+=-I$(OBJDIR)
CPPFLAGS=$(CFLAGS)

all:	 $(BINDIR)/proploader$(EXT) $(BUILD)/blink-fast.binary $(BUILD)/blink-slow.binary $(BUILD)/toggle.elf

$(OBJS):	$(OBJDIR)/created $(HDRS) $(OBJDIR)/IP_Loader.h Makefile

$(BINDIR)/proploader$(EXT):	$(BINDIR)/created $(OBJS)
	$(CPP) -o $@ $(LDFLAGS) $(OBJS) $(LIBS) -lstdc++

$(BUILD)/%.elf:	%.c
	propeller-elf-gcc -Os -mlmm -o $@ $<
    
$(BUILD)/%-fast.binary:	%.spin
	$(SPINCMP) -o $@ $<

$(BUILD)/%-slow.binary:	%.spin
	$(SPINCMP) -DSLOW -o $@ $<

$(OBJDIR)/%.binary:	$(SPINDIR)/%.spin
	$(SPINCMP) -o $@ $<

$(OBJDIR)/%.c:	$(OBJDIR)/%.binary $(BINDIR)/bin2c$(EXT)
	$(BINDIR)/bin2c$(EXT) $< $@

$(OBJDIR)/%.o:	$(OBJDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/IP_Loader.h:   $(SPINDIR)/IP_Loader.spin $(BINDIR)/split$(EXT)
	$(SPINCMP) -o $(OBJDIR)/IP_Loader.binary $<
	$(BINDIR)/split$(EXT) $(OBJDIR)/IP_Loader.binary $(OBJDIR)/IP_Loader.h

setup:	$(BUILD)/blink-slow.binary
	propeller-load -e $(BUILD)/blink-slow.binary

run:	$(BINDIR)/proploader$(EXT) $(BUILD)/blink-fast.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/blink-fast.binary -t

runbig:	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf
	$(BINDIR)/proploader$(EXT) $(BUILD)/toggle.elf -t

E:	$(BINDIR)/proploader$(EXT) $(BUILD)/blink-fast.binary
	$(BINDIR)/proploader$(EXT) $(BUILD)/blink-fast.binary -e

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

$(BINDIR)/%$(EXT):	$(TOOLDIR)/%.c
	$(TOOLCC) $(CFLAGS) $< -o $@

clean:
	$(RM) $(BUILD)

%/created:
	@$(MKDIR) -p $(@D)
	@$(TOUCH) $@
