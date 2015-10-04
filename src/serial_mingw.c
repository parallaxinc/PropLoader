/*
 * osint_mingw.c - serial i/o routines for win32api via mingw
 *
 * Based on: Serial I/O functions used by PLoadLib.c
 *
 * Copyright (c) 2009 by John Steven Denson
 * Modified in 2011 by David Michael Betz
 * Modified in 2015 by David Michael Betz
 *
 * MIT License                                                           
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include <windows.h>

#include <conio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "serial.h"

static void ShowLastError(void);

struct SERIAL {
    COMMTIMEOUTS originalTimeouts;
    COMMTIMEOUTS timeouts;
    reset_method_t resetMethod;
    HANDLE hSerial;
};

int SerialUseResetMethod(SERIAL *serial, char *method)
{
    if (strcasecmp(method, "dtr") == 0)
        serial->resetMethod = RESET_WITH_DTR;
    else if (strcasecmp(method, "rts") == 0)
       serial->resetMethod = RESET_WITH_RTS;
    else
        return -1;
    return 0;
}

int OpenSerial(const char *port, int baud, SERIAL **pSerial)
{
    char fullPort[20];
    SERIAL *serial;
    DCB state;
    int sts;

    /* allocate a serial state structure */
    if (!(serial = (SERIAL *)malloc(sizeof(SERIAL))))
        return -1;
        
    /* initialize the state structure */
    memset(serial, 0, sizeof(SERIAL));
    serial->resetMethod = RESET_WITH_DTR;

    sprintf(fullPort, "\\\\.\\%s", port);

    serial->hSerial = CreateFile(
        fullPort,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (serial->hSerial == INVALID_HANDLE_VALUE) {
        free(serial);
        return -1;
    }

    /* set the baud rate */
    if ((sts = SetSerialBaud(serial, baud)) != 0) {
        CloseHandle(serial->hSerial);
        free(serial);
        return sts;
    }

    GetCommState(serial->hSerial, &state);
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    state.fOutxDsrFlow = FALSE;
    state.fDtrControl = DTR_CONTROL_DISABLE;
    state.fOutxCtsFlow = FALSE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    state.fInX = FALSE;
    state.fOutX = FALSE;
    state.fBinary = TRUE;
    state.fParity = FALSE;
    state.fDsrSensitivity = FALSE;
    state.fTXContinueOnXoff = TRUE;
    state.fNull = FALSE;
    state.fAbortOnError = FALSE;
    SetCommState(serial->hSerial, &state);

    GetCommTimeouts(serial->hSerial, &serial->originalTimeouts);
    serial->timeouts = serial->originalTimeouts;
    serial->timeouts.ReadIntervalTimeout = MAXDWORD;
    serial->timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    serial->timeouts.WriteTotalTimeoutMultiplier = 0;
    serial->timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(serial->hSerial, &serial->timeouts);

    /* setup device buffers */
    SetupComm(serial->hSerial, 10000, 10000);

    /* purge any information in the buffer */
    PurgeComm(serial->hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

    /* return the serial state structure */
    *pSerial = serial;
    return 0;
}

void CloseSerial(SERIAL *serial)
{
    if (serial->hSerial != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(serial->hSerial);
        CloseHandle(serial->hSerial);
    }
    free(serial);
}

int SetSerialBaud(SERIAL *serial, int baud)
{
    DCB state;

    GetCommState(serial->hSerial, &state);
    switch (baud) {
    case 9600:
        state.BaudRate = CBR_9600;
        break;
    case 19200:
        state.BaudRate = CBR_19200;
        break;
    case 38400:
        state.BaudRate = CBR_38400;
        break;
    case 57600:
        state.BaudRate = CBR_57600;
        break;
    case 115200:
        state.BaudRate = CBR_115200;
        break;
    case 128000:
        state.BaudRate = CBR_128000;
        break;
    case 256000:
        state.BaudRate = CBR_256000;
        break;
    default:
        /* just try the number the user entered */
        state.BaudRate = baud;
        break;
    }
    SetCommState(serial->hSerial, &state);
    
    return 0;
}

int SerialGenerateResetSignal(SERIAL *serial)
{
    EscapeCommFunction(serial->hSerial, serial->resetMethod == RESET_WITH_RTS ? SETRTS : SETDTR);
    Sleep(25);
    EscapeCommFunction(serial->hSerial, serial->resetMethod == RESET_WITH_RTS ? CLRRTS : CLRDTR);
    Sleep(90);
    // Purge here after reset helps to get rid of buffered data.
    PurgeComm(serial->hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    return 0;
}

int SendSerialData(SERIAL *serial, const void *buf, int len)
{
    DWORD dwBytes = 0;
    if (!WriteFile(serial->hSerial, buf, len, &dwBytes, NULL)) {
        printf("Error writing port\n");
        ShowLastError();
        return -1;
    }
    return dwBytes;
}

int FlushSerialData(SERIAL *serial)
{
    return FlushFileBuffers(serial->hSerial) ? 0 : -1;
}

int ReceiveSerialData(SERIAL *serial, void *buf, int len)
{
    DWORD dwBytes = 0;
    FlushFileBuffers(serial->hSerial);
    serial->timeouts.ReadTotalTimeoutConstant = 0;
    SetCommTimeouts(serial->hSerial, &serial->timeouts);
    if (!ReadFile(serial->hSerial, buf, len, &dwBytes, NULL)) {
        printf("Error reading port\n");
        ShowLastError();
        return -1;
    }
    return dwBytes;
}

int ReceiveSerialDataTimeout(SERIAL *serial, void *buf, int len, int timeout)
{
    DWORD dwBytes = 0;
    FlushFileBuffers(serial->hSerial);
    serial->timeouts.ReadTotalTimeoutConstant = timeout;
    SetCommTimeouts(serial->hSerial, &serial->timeouts);
    if (!ReadFile(serial->hSerial, buf, len, &dwBytes, NULL)) {
        printf("Error reading port\n");
        ShowLastError();
        return -1;
    }
    
    if (dwBytes == 0) {
        //printf("Timeout 1\n");
        return -1;
    }
    
    return dwBytes;
}

int ReceiveSerialDataExact(SERIAL *serial, void *buf, int len, int timeout)
{
    uint8_t *ptr = (uint8_t *)buf;
    int remaining = len;
    DWORD dwBytes = 0;
    
    FlushFileBuffers(serial->hSerial);

    serial->timeouts.ReadTotalTimeoutConstant = timeout;
    SetCommTimeouts(serial->hSerial, &serial->timeouts);
    
    /* return only when the buffer contains the exact amount of data requested */
    while (remaining > 0) {
    
        /* read the next bit of data */
        if (!ReadFile(serial->hSerial, ptr, remaining, &dwBytes, NULL)) {
            printf("Error reading port\n");
            ShowLastError();
            return -1;
        }
        
        /* check for a timeout */
        if (dwBytes == 0) {
            //printf("Timeout %d %d\n", len, remaining);
            return -1;
        }
                    
        /* update the buffer pointer */
        remaining -= dwBytes;
        ptr += dwBytes;
    }

    /* return the full size of the buffer */
    return len;
}

static void ShowLastError(void)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);
    printf("    %s\n", (char *)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

/* escape from terminal mode */
#define ESC         0x1b

/*
 * if "check_for_exit" is true, then
 * a sequence EXIT_CHAR 00 nn indicates that we should exit
 */
#define EXIT_CHAR   0xff

void SerialTerminal(SERIAL *serial, int check_for_exit, int pst_mode)
{
    int sawexit_char = 0;
    int sawexit_valid = 0;
    int exitcode = 0;
    int continue_terminal = 1;

    while (continue_terminal) {
        uint8_t buf[1];
        if (ReceiveSerialDataTimeout(serial, buf, 1, 0) != -1) {
            if (sawexit_valid) {
                exitcode = buf[0];
                continue_terminal = 0;
            }
            else if (sawexit_char) {
                if (buf[0] == 0) {
                    sawexit_valid = 1;
                } else {
                    putchar(EXIT_CHAR);
                    putchar(buf[0]);
                    fflush(stdout);
                }
            }
            else if (check_for_exit && buf[0] == EXIT_CHAR) {
                sawexit_char = 1;
            }
            else {
                putchar(buf[0]);
                if (pst_mode && buf[0] == '\r')
                    putchar('\n');
                fflush(stdout);
            }
        }
        else if (kbhit()) {
            if ((buf[0] = getch()) == ESC)
                break;
            SendSerialData(serial, buf, 1);
        }
    }

    if (check_for_exit && sawexit_valid) {
        exit(exitcode);
    }
}

#if 0

HANDLE hComm;
hComm = CreateFile( gszPort,  
                    GENERIC_READ | GENERIC_WRITE, 
                    0, 
                    0, 
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    0);
if (hComm == INVALID_HANDLE_VALUE)
   // error opening port; abort
   
DWORD dwRead;
BOOL fWaitingOnRead = FALSE;
OVERLAPPED osReader = {0};

// Create the overlapped event. Must be closed before exiting
// to avoid a handle leak.
osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

if (osReader.hEvent == NULL)
   // Error creating overlapped event; abort.

if (!fWaitingOnRead) {
   // Issue read operation.
   if (!ReadFile(hComm, lpBuf, READ_BUF_SIZE, &dwRead, &osReader)) {
      if (GetLastError() != ERROR_IO_PENDING)     // read not delayed?
         // Error in communications; report it.
      else
         fWaitingOnRead = TRUE;
   }
   else {    
      // read completed immediately
      HandleASuccessfulRead(lpBuf, dwRead);
    }
}

#define READ_TIMEOUT      500      // milliseconds

DWORD dwRes;

if (fWaitingOnRead) {
   dwRes = WaitForSingleObject(osReader.hEvent, READ_TIMEOUT);
   switch(dwRes)
   {
      // Read completed.
      case WAIT_OBJECT_0:
          if (!GetOverlappedResult(hComm, &osReader, &dwRead, FALSE))
             // Error in communications; report it.
          else
             // Read completed successfully.
             HandleASuccessfulRead(lpBuf, dwRead);

          //  Reset flag so that another opertion can be issued.
          fWaitingOnRead = FALSE;
          break;

      case WAIT_TIMEOUT:
          // Operation isn't complete yet. fWaitingOnRead flag isn't
          // changed since I'll loop back around, and I don't want
          // to issue another read until the first one finishes.
          //
          // This is a good time to do some background work.
          break;                       

      default:
          // Error in the WaitForSingleObject; abort.
          // This indicates a problem with the OVERLAPPED structure's
          // event handle.
          break;
   }
}

#endif
