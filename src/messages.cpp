#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "messages.h"

enum {
    MIN_INFO = 1,
    INFO_OPENING_FILE   = MIN_INFO,
    INFO_DOWNLOADING,
    INFO_VERIFYING_RAM,
    MAX_INFO,
    MIN_ERROR           = 100,
    ERROR_OPTION_N      = MIN_ERROR,
    MAX_ERROR
    
};

// message codes 1-99
static const char *infoText[] = {
"Opening file '%s'",
"Downloading file to port %s",
"Verifying RAM",
"Programming EEPROM",
"Download successful!",
"[ Entering terminal mode. Type ESC or Control-C to exit. ]",
"Writing '%s' to the SD card",
"%ld bytes remaining             ",
"%ld bytes sent                  ",
"Setting module name to '%s'"
};

// message codes 100 and up
static const char *errorText[] = {
"Option -n can only be used to name wifi modules",
"Invalid address: %s",
"Download failed: %d",
"Can't open file '%s'",
"Propeller not found on port %s",
"Failed to enter terminal mode",
"Unrecognized wi-fi module firmware\n\
    Version is %s but expected %s.\n\
    Recommended action: update firmware and/or PropLoader to latest version(s).",
"Failed to write SD card file '%s'",
"Invalid module name",
"Failed to set module name",
"File is truncated or not a Propeller application image",
"File is corrupt or not a Propeller application",
"Can't read Propeller application file '%s'",
"Wifi module discovery failed",
"No wifi modules found",
"Serial port discovery failed",
"No serial ports found",
"Unable to connect to port %s",
"Unable to connect to module at %s",
"Failed to set baud rate",
"Internal error",
"Insufficient memory"
};

static const char *messageText(int messageNumber)
{
    const char *fmt;
    if (messageNumber >= MIN_INFO && messageNumber < MAX_INFO)
        fmt = infoText[messageNumber - MIN_INFO];
    else if (messageNumber >= MIN_ERROR && messageNumber < MAX_ERROR)
        fmt = errorText[messageNumber - MIN_ERROR];
    else
        fmt = "Internal error";
    return fmt;
}

int verbose = 0;
int showMessageCodes = false;

int error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmessage(fmt, ap, '\n');
    va_end(ap);
    return -1;
}

int nerror(int messageNumber, ...)
{
    va_list ap;
    va_start(ap, messageNumber);
    vmessage(messageText(messageNumber), ap, '\n');
    va_end(ap);
    return -1;
}

void message(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmessage(fmt, ap, '\n');
    va_end(ap);
}

void nmessage(int messageNumber, ...)
{
    va_list ap;
    va_start(ap, messageNumber);
    vmessage(messageText(messageNumber), ap, '\n');
    va_end(ap);
}

void progress(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmessage(fmt, ap, '\r');
    va_end(ap);
}

void vmessage(const char *fmt, va_list ap, int eol)
{
    const char *p = fmt;
    int code = 0;

    /* check for and parse the numeric message code */
    if (*p && isdigit(*p)) {
        while (*p && isdigit(*p))
            code = code * 10 + *p++ - '0';
        if (*p == '-')
            fmt = ++p;
    }

    /* display messages in verbose mode or when the code is > 0 */
    if (verbose || code > 0) {
        if (showMessageCodes)
            printf("%03d-", code);
        if (code > 99)
            printf("ERROR: ");
        vprintf(fmt, ap);
        putchar(eol);
        if (eol == '\r')
            fflush(stdout);
    }
}
