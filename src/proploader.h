#ifndef __PROPLOADER_H__
#define __PROPLOADER_H__

#include "messages.h"

#define RCFAST      0x00
#define RCSLOW      0x01
#define XINPUT      0x22
#define XTAL1       0x2a
#define XTAL2       0x32
#define XTAL3       0x3a
#define PLL1X       0x41
#define PLL2X       0x42
#define PLL4X       0x43
#define PLL8X       0x44
#define PLL16X      0x45

#define DEF_LOADER_BAUDRATE         115200
#define DEF_FAST_LOADER_BAUDRATE    921600
#define DEF_TERMINAL_BAUDRATE       115200
#define DEF_CLOCK_SPEED             80000000
#define DEF_CLOCK_MODE              (XTAL1+PLL16X)

#endif
