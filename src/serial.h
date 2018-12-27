/**
 * @file osint.h
 *
 * Serial I/O functions used by PLoadLib.c
  *
 * Copyright (c) 2009 by John Steven Denson
 * Modified in 2011 by David Michael Betz
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
#ifndef __SERIAL_IO_H__
#define __SERIAL_IO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Method of issuing reset to the Propeller chip. */
typedef enum {
    RESET_WITH_RTS,
    RESET_WITH_DTR,
    RESET_WITH_GPIO
} reset_method_t;

typedef struct SERIAL SERIAL;

int SerialUseResetMethod(SERIAL *serial, const char *method);
int OpenSerial(const char *port, int baud, SERIAL **pSerial);
void CloseSerial(SERIAL *serial);
int SetSerialBaud(SERIAL *serial, int baud);
int SerialGenerateResetSignal(SERIAL *serial);
int SendSerialData(SERIAL *serial, const void *buf, int len);
int FlushSerialData(SERIAL *serial);
int ReceiveSerialData(SERIAL *serial, void *buf, int len);
int ReceiveSerialDataTimeout(SERIAL *serial, void *buf, int len, int timeout);
int ReceiveSerialDataExactTimeout(SERIAL *serial, void *buf, int len, int timeout);
int SerialFind(int (*check)(const char *port, void *data), void *data);
void SerialTerminal(SERIAL *serial, int check_for_exit, int pst_mode);

#ifdef __cplusplus
}
#endif

#endif
