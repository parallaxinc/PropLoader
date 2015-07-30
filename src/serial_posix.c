/**
 * @file osint_linux.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>
#include <sys/select.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>

#include "serial.h"

struct SERIAL {
    struct termios oldParams;
    reset_method_t resetMethod;
#ifdef RASPBERRY_PI
    int propellerResetGpioPin;
    int propellerResetGpioLevel;
#endif
    int fd;
};

/* serial i/o definitions */
#define SERIAL_TIMEOUT  -1

static void msleep(int ms)
{
    usleep(ms * 1000);
}

static void chk(char *fun, int sts)
{
    if (sts != 0)
        printf("%s failed\n", fun);
}

int SerialUseResetMethod(SERIAL *serial, char *method)
{
    if (strcasecmp(method, "dtr") == 0)
        serial->resetMethod = RESET_WITH_DTR;
    else if (strcasecmp(method, "rts") == 0)
       serial->resetMethod = RESET_WITH_RTS;
#ifdef RASPBERRY_PI
    else if (strncasecmp(method, "gpio", 4) == 0)
    {
        serial->resetMethod = RESET_WITH_GPIO;

        char *token;
        token = strtok(method, ",");
        if (token)
        {
            token = strtok(NULL, ",");
            if (token)
            {
                serial->resetGpioPin = atoi(token);
            }
            token = strtok(NULL, ",");
            if (token)
            {
                serial->resetGpioLevel = atoi(token); 
            }
        }

        //printf ("Using GPIO pin %d as Propeller reset ", propellerResetGpioPin);
        if (propellerResetGpioLevel)
        {
            printf ("(HIGH).\n");
        }
        else
        {
            printf ("(LOW).\n");
        }
        gpio_export(serial->resetGpioPin);
        gpio_write(serial->resetGpioPin, serial->resetGpioLevel ^ 1);
        gpio_direction(serial->resetGpioPin, 1);
    }
#endif
    else {
        return -1;
    }
    return 0;
}

int OpenSerial(const char *port, int baud, SERIAL **pSerial)
{
    struct termios sparams;
    SERIAL *serial;
    int sts;
    
    /* allocate a serial state structure */
    if (!(serial = (SERIAL *)malloc(sizeof(SERIAL))))
        return -2;
        
    /* initialize the state structure */
    memset(serial, 0, sizeof(SERIAL));
    serial->resetMethod = RESET_WITH_DTR;
#ifdef RASPBERRY_PI
    serial->resetGpioPin = 17;
    serial->resetGpioLevel = 0;
#endif
    serial->fd = -1;
        
    /* open the port */
#ifdef MACOSX
    serial->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
#else
    serial->fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
#endif
    if(serial->fd == -1) {
        printf("error: opening '%s' -- %s\n", port, strerror(errno));
        free(serial);
        return -3;
    }

#if 0    
    signal(SIGINT, sigint_handler);
#endif

    /* set the terminal to exclusive mode */
    if (ioctl(serial->fd, TIOCEXCL) != 0) {
        printf("error: can't open terminal for exclusive access\n");
        close(serial->fd);
        free(serial);
        return -4;
    }

    fcntl(serial->fd, F_SETFL, 0);
    
    /* set the baud rate */
    if ((sts = SetSerialBaud(serial, baud)) != 0) {
        close(serial->fd);
        free(serial);
        return sts;
    }

    /* get the current options */
    chk("tcgetattr", tcgetattr(serial->fd, &serial->oldParams));
    sparams = serial->oldParams;
    
    /* set raw input */
#ifdef MACOSX
    cfmakeraw(&sparams);
    sparams.c_cc[VTIME] = 0;
    sparams.c_cc[VMIN] = 1;
#else
    memset(&sparams, 0, sizeof(sparams));
    sparams.c_cflag     = CS8 | CLOCAL | CREAD;
    sparams.c_lflag     = 0; // &= ~(ICANON | ECHO | ECHOE | ISIG);
    sparams.c_oflag     = 0; // &= ~OPOST;

    sparams.c_iflag     = IGNPAR | IGNBRK;
    sparams.c_cc[VTIME] = 0;
    sparams.c_cc[VMIN]  = 1;
#endif

    /* set the options */
    chk("tcflush", tcflush(serial->fd, TCIFLUSH));
    chk("tcsetattr", tcsetattr(serial->fd, TCSANOW, &sparams));

    /* return the serial state structure */
    *pSerial = serial;
    return 0;
}

void CloseSerial(SERIAL *serial)
{
    if (serial->fd != -1) {
        tcflush(serial->fd, TCIOFLUSH);
        tcsetattr(serial->fd, TCSANOW, &serial->oldParams);
        ioctl(serial->fd, TIOCNXCL);
        close(serial->fd);
    }
    free(serial);
}

int SetSerialBaud(SERIAL *serial, int baud)
{
    struct termios sparams;
    int tbaud = 0;

    switch(baud) {
    case 0: // default
        tbaud = B115200;
        break;
#ifdef B921600
    case 921600:
        tbaud = B921600;
        break;
#endif
#ifdef B576000
    case 576000:
        tbaud = B576000;
        break;
#endif
#ifdef B500000
    case 500000:
        tbaud = B500000;
        break;
#endif
#ifdef B460800
    case 460800:
        tbaud = B460800;
        break;
#endif
#ifdef B230400
    case 230400:
        tbaud = B230400;
        break;
#endif
    case 115200:
        tbaud = B115200;
        break;
    case 57600:
        tbaud = B57600;
        break;
    case 38400:
        tbaud = B38400;
        break;
    case 19200:
        tbaud = B19200;
        break;
    case 9600:
        tbaud = B9600;
        break;
    default:
        tbaud = baud; break;
        printf("Unsupported baudrate. Use ");
#ifdef B921600
        printf("921600, ");
#endif
#ifdef B576000
        printf("576000, ");
#endif
#ifdef B500000
        printf("500000, ");
#endif
#ifdef B460800
        printf("460800, ");
#endif
#ifdef B230400
        printf("230400, ");
#endif
        printf("115200, 57600, 38400, 19200, or 9600\n");
        return -1;
    }
    
    /* get the current options */
    chk("tcgetattr", tcgetattr(serial->fd, &sparams));
    
    /* set raw input */
#ifdef MACOSX
    chk("cfsetspeed", cfsetspeed(&sparams, tbaud));
#else
    chk("cfsetispeed", cfsetispeed(&sparams, tbaud));
    chk("cfsetospeed", cfsetospeed(&sparams, tbaud));
#endif

    /* set the options */
    chk("tcflush", tcflush(serial->fd, TCIFLUSH));
    chk("tcsetattr", tcsetattr(serial->fd, TCSANOW, &sparams));
    
    return 0;
}

int SerialGenerateResetSignal(SERIAL *serial)
{
    int cmd;
    
    /* assert the reset signal */
    switch (serial->resetMethod) {
    case RESET_WITH_DTR:
        cmd = TIOCM_DTR;
        ioctl(serial->fd, TIOCMBIS, &cmd); /* set bit */
        break;
    case RESET_WITH_RTS:
        cmd = TIOCM_RTS;
        ioctl(serial->fd, TIOCMBIS, &cmd); /* set bit */
        break;
#ifdef RASPBERRY_PI
    case RESET_WITH_GPIO:
        gpio_write(serial->resetGpioPin, serial->resetGpioLevel);
        break;
#endif
    default:
        // should be reached
        break;
    }

    msleep(10);
    
    /* deassert the reset signal */
    switch (serial->resetMethod) {
    case RESET_WITH_DTR:
        cmd = TIOCM_DTR;
        ioctl(serial->fd, TIOCMBIC, &cmd); /* clear bit */
        break;
    case RESET_WITH_RTS:
        cmd = TIOCM_RTS;
        ioctl(serial->fd, TIOCMBIC, &cmd); /* clear bit */
        break;
#ifdef RASPBERRY_PI
    case RESET_WITH_GPIO:
        gpio_write(serial->resetGpioPin, serial->resetGpioLevel ^ 1);
        break;
#endif
    default:
        // should be reached
        break;
    }

    msleep(100);
    
    /* flush any pending input */
    tcflush(serial->fd, TCIFLUSH);
    
    return 0;
}

int SendSerialData(SERIAL *serial, const void *buf, int len)
{
    int cnt;
    cnt = write(serial->fd, buf, len);
    if (cnt != len) {
        printf("Error writing port\n");
        return -1;
    }
    return cnt;
}

int ReceiveSerialData(SERIAL *serial, void *buf, int len)
{
    int cnt = read(serial->fd, buf, len);
    if (cnt < 1) {
        printf("Error reading port\n");
        return -1;
    }
    return cnt;
}

int ReceiveSerialDataExact(SERIAL *serial, void *buf, int len, int timeout)
{
    uint8_t *ptr = (uint8_t *)buf;
    int remaining = len;
    int cnt = 0;
    struct timeval toval;
    fd_set set;

    /* return only when the buffer contains the exact amount of data requested */
    while (remaining > 0) {
        
        /* initialize the socket set */
        FD_ZERO(&set);
        FD_SET(serial->fd, &set);

        /* setup the timeout */
        toval.tv_sec = timeout / 1000;
        toval.tv_usec = (timeout % 1000) * 1000;

        /* wait for data to be available on the port */
        if (select(serial->fd + 1, &set, NULL, NULL, &toval) > 0) {
            if (FD_ISSET(serial->fd, &set)) {
            
                /* read the next bit of data */
                if ((cnt = read(serial->fd, ptr, remaining)) < 0)
                    return -1;
                    
                /* update the buffer pointer */
                remaining -= cnt;
                ptr += cnt;
            }
        }
    }

    /* return the full size of the buffer */
    return len;
}

#if 0

int serial_find(const char* prefix, int (*check)(const char* port, void* data), void* data)
{
    char path[PATH_MAX];
    int prefixlen = strlen(prefix);
    struct dirent *entry;
    DIR *dirp;
    
    if (!(dirp = opendir("/dev")))
        return -1;
    
    while ((entry = readdir(dirp)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefixlen) == 0) {
            sprintf(path, "/dev/%s", entry->d_name);
            if ((*check)(path, data) == 0) {
                closedir(dirp);
                return 0;
            }
        }
    }
    
    closedir(dirp);
    return -1;
}

static void sigint_handler(int signum)
{
    serial_done();
    exit(1);
}

#define ESC     0x1b    /* escape from terminal mode */

/**
 * simple terminal emulator
 */
void terminal_mode(int check_for_exit, int pst_mode)
{
    struct termios oldt, newt;
    char buf[128], realbuf[256]; // double in case buf is filled with \r in PST mode
    ssize_t cnt;
    fd_set set;
    int exit_char = 0xdead; /* not a valid character */
    int sawexit_char = 0;
    int sawexit_valid = 0; 
    int exitcode = 0;
    int continue_terminal = 1;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_iflag &= ~(ICRNL | INLCR);
    newt.c_oflag &= ~OPOST;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (check_for_exit)
      {
        exit_char = 0xff;
      }

#if 0
    /* make it possible to detect breaks */
    tcgetattr(serial->fd, &newt);
    newt.c_iflag &= ~IGNBRK;
    newt.c_iflag |= PARMRK;
    tcsetattr(serial->fd, TCSANOW, &newt);
#endif

    do {
        FD_ZERO(&set);
        FD_SET(serial->fd, &set);
        FD_SET(STDIN_FILENO, &set);
        if (select(serial->fd + 1, &set, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(serial->fd, &set)) {
                if ((cnt = read(serial->fd, buf, sizeof(buf))) > 0) {
                    int i;
                    // check for breaks
                    ssize_t realbytes = 0;
                    for (i = 0; i < cnt; i++) {
                      if (sawexit_valid)
                        {
                          exitcode = buf[i];
                          continue_terminal = 0;
                        }
                      else if (sawexit_char) {
                        if (buf[i] == 0) {
                          sawexit_valid = 1;
                        } else {
                          realbuf[realbytes++] = exit_char;
                          realbuf[realbytes++] = buf[i];
                          sawexit_char = 0;
                        }
                      } else if (((int)buf[i] & 0xff) == exit_char) {
                        sawexit_char = 1;
                      } else {
                        realbuf[realbytes++] = buf[i];
                        if (pst_mode && buf[i] == '\r')
                            realbuf[realbytes++] = '\n';
                      }
                    }
                    write(fileno(stdout), realbuf, realbytes);
                }
            }
            if (FD_ISSET(STDIN_FILENO, &set)) {
                if ((cnt = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                    int i;
                    for (i = 0; i < cnt; ++i) {
                        //printf("%02x\n", buf[i]);
                        if (buf[i] == ESC)
                            goto done;
                    }
                    write(serial->fd, buf, cnt);
                }
            }
        }
    } while (continue_terminal);

done:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (sawexit_valid)
      {
        exit(exitcode);
      }
    
}

#endif
