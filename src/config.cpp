#include <string.h>
#include "config.h"

/* this is a hack that only works for the ActivityBoard at present */

int GetNumericConfigField(BoardConfig *config, const char *tag, int *pValue)
{
    if (strcmp(tag, "clkfreq") == 0)
        *pValue = 80000000;
    else if (strcmp(tag, "clkmode") == 0)
        *pValue = XTAL1+PLL16X;
    else if (strcmp(tag, "baudrate") == 0)
        *pValue = 115200;
    else if (strcmp(tag, "rxpin") == 0)
        *pValue = 31;
    else if (strcmp(tag, "txpin") == 0)
        *pValue = 30;
    else if (strcmp(tag, "tvpin") == 0)
        *pValue = 0;
    else if (strcmp(tag, "sdspi-do") == 0)
        *pValue = 22;
    else if (strcmp(tag, "sdspi-clk") == 0)
        *pValue = 23;
    else if (strcmp(tag, "sdspi-di") == 0)
        *pValue = 24;
    else if (strcmp(tag, "sdspi-cs") == 0)
        *pValue = 25;
    else
        return false;

    return true;
}

