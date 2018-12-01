' select debugging output to a TV
'#define TV_DEBUG

CON
  ' these will get overwritten with the correct values before loading
  _clkmode    = xtal1 + pll16x
  _xinfreq    = 5_000_000

  TYPE_FILE_WRITE = 0
  TYPE_DATA = 1
  TYPE_EOF = 2

  ' character codes
  CR = $0d

  WRITE_NONE = 0
  WRITE_FILE = 1

OBJ
  pkt : "packet_driver"
#ifdef TV_DEBUG
  tv   : "TV_Text"
#endif
  sd   : "fsrw"

VAR
  long sd_mounted
  long load_address
  long write_mode

PUB start | type, packet, len, ok

  ' allow some time for the PC to change baud rate if necesssary
  waitcnt(CLKFREQ / 2 + CNT)

  ' start the packet driver
  pkt.start(31, 30, 0, p_baudrate)

#ifdef TV_DEBUG
  tv.start(p_tvpin)
  tv.str(string("Serial Helper v0.1", CR))
#endif

  ' initialize
  sd_mounted := 0
  write_mode := WRITE_NONE

  ' handle packets
  repeat

    ' get the next packet from the PC
    ok := pkt.rcv_packet(@type, @packet, @len)
#ifdef TV_DEBUG
    if not ok
      tv.str(string("Receive packet error", CR))
#endif

    if ok
      case type
        TYPE_FILE_WRITE:        FILE_WRITE_handler(packet)
        TYPE_DATA:              DATA_handler(packet, len)
        TYPE_EOF:               EOF_handler
        other:
#ifdef TV_DEBUG
          tv.str(string("Bad packet type: "))
          tv.hex(type, 2)
          crlf
#endif
      pkt.release_packet

PRI FILE_WRITE_handler(name) | err
  mountSD
#ifdef TV_DEBUG
  tv.str(string("FILE_WRITE: "))
  tv.str(name)
  tv.str(string("...",))
#endif
  err := \sd.popen(name, "w")
#ifdef TV_DEBUG
  tv.dec(err)
  crlf
#endif
  write_mode := WRITE_FILE
  load_address := $00000000

PRI DATA_handler(packet, len) | i, val, err
  err := 0 ' assume no error
#ifdef TV_DEBUG
  tv.str(string("DATA: "))
  tv.hex(load_address, 8)
  tv.out(" ")
  tv.dec(len)
  tv.str(string("..."))
#endif
  case write_mode
    WRITE_FILE:
      err := \sd.pwrite(packet, len)
      load_address += len
    other:
#ifdef TV_DEBUG
      tv.str(string("bad write_mode: "))
#endif
#ifdef TV_DEBUG
  tv.dec(err)
  crlf
#endif

PRI EOF_handler | err
  err := 0 ' assume no errors
#ifdef TV_DEBUG
  tv.str(string("EOF..."))
#endif
  case write_mode
    WRITE_FILE:
      err := \sd.pclose
      write_mode := WRITE_NONE
    WRITE_NONE:
    other:
#ifdef TV_DEBUG
      tv.str(string("bad write_mode: "))
#endif
#ifdef TV_DEBUG
  tv.dec(err)
  crlf
#endif

PRI mountSD | err
  if sd_mounted == 0
    repeat
#ifdef TV_DEBUG
      tv.str(string("Mounting SD card...", CR))
#endif
      err := \sd.mount_explicit(p_dopin, p_clkpin, p_dipin, p_cspin, p_sel_inc, p_sel_msk, p_sel_addr)
    until err == 0
    sd_mounted := 1
  return 1

#ifdef TV_DEBUG
PRI crlf
  tv.out(CR)
#endif

DAT

' parameters filled in before downloading sd_helper.binary
p_baudrate          long    0
p_tvpin             byte    0
p_dopin             byte    0
p_clkpin            byte    0
p_dipin             byte    0
p_cspin             byte    0
p_sel_addr          byte    0
p_sel_inc           long    0
p_sel_msk           long    0

