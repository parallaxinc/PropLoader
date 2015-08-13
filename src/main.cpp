#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include <iostream>

#include "loadelf.h"
#include "xbee-loader.hpp"
#include "serial-loader.hpp"

/* port prefix */
#if defined(CYGWIN) || defined(WIN32) || defined(MINGW)
  #define PORT_PREFIX ""
#elif defined(LINUX)
  #ifdef RASPBERRY_PI
    #define PORT_PREFIX "ttyAMA"
  #else
    #define PORT_PREFIX "ttyUSB"
  #endif
#elif defined(MACOSX)
  #define PORT_PREFIX "cu.usbserial-"
#else
  #define PORT_PREFIX ""
#endif

static void usage(const char *progname)
{
printf("\
usage: %s\n\
         [ -b <baudrate> ]  initial baud rate (default is %d)\n\
         [ -i <ip-addr> ]   IP address of the Xbee Wi-Fi module\n\
         [ -p <port> ]      serial port\n\
         [ -P ]             show all serial ports with propellers connected\n\
         [ -P0 ]            show all serial ports\n\
         [ -s ]             do a serial download\n\
         [ -t ]             enter terminal mode after the load is complete\n\
         [ -X ]             show all discovered xbee modules with propellers connected\n\
         [ -X0 ]            show all discovered xbee modules\n\
         [ -? ]             display a usage message and exit\n\
         <file>             spin binary file to load\n", progname, DEFAULT_BAUDRATE);
    exit(1);
}

void ShowPorts(const char *prefix, bool check);
void ShowXbeeModules(bool check);
uint8_t *LoadSpinBinaryFile(FILE *fp, int *pLength);
uint8_t *LoadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize);

int main(int argc, char *argv[])
{
    bool done = false;
    bool terminalMode = false;
    const char *ipaddr = NULL;
    const char *port = NULL;
    int baudrate = DEFAULT_BAUDRATE;
    bool useSerial = false;
    char *file = NULL;
    XbeeLoader xbeeLoader;
    SerialLoader serialLoader;
    Loader *loader;
    int sts, i;
    
    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'b':
                if (argv[i][2])
                    baudrate = atoi(&argv[i][2]);
                else if (++i < argc)
                    baudrate = atoi(argv[i]);
                else
                    usage(argv[0]);
                break;
            case 'i':   // set the ip address
                if (argv[i][2])
                    ipaddr = &argv[i][2];
                else if (++i < argc)
                    ipaddr = argv[i];
                else
                    usage(argv[0]);
                useSerial = false;
                break;
            case 'p':
                if (argv[i][2])
                    port = &argv[i][2];
                else if (++i < argc)
                    port = argv[i];
                else
                    usage(argv[0]);
#if defined(CYGWIN) || defined(WIN32) || defined(LINUX)
                if (isdigit((int)port[0])) {
#if defined(CYGWIN) || defined(WIN32)
                    static char buf[10];
                    sprintf(buf, "COM%d", atoi(port));
                    port = buf;
#endif
#if defined(LINUX)
                    static char buf[64];
                    sprintf(buf, "/dev/%s%d", PORT_PREFIX, atoi(port));
                    port = buf;
#endif
                }
#endif
#if defined(MACOSX)
                if (port[0] != '/') {
                    static char buf[64];
                    sprintf(buf, "/dev/%s-%s", PORT_PREFIX, port);
                    port = buf;
                }
#endif
                useSerial = true;
                break;
            case 'P':
                ShowPorts(PORT_PREFIX, argv[i][2] == '0' ? false : true);
                done = true;
                break;
            case 's':
                useSerial = true;
                break;
            case 't':
                terminalMode = true;
                break;
            case 'X':
                ShowXbeeModules(argv[i][2] == '0' ? false : true);
                done = true;
                break;
            default:
                usage(argv[0]);
                break;
            }
        }
        
        /* remember the file to load */
        else {
            if (file)
                usage(argv[0]);
            file = argv[i];
        }
    }
    
    /* make sure a file to load was specified */
    if (!done && !file)
        usage(argv[0]);
        
    /* check to see if there is a file to load */
    if (!file)
        goto finish;

    /* do a serial download */
    if (useSerial) {
        SerialInfo info;
        if (!port) {
            SerialInfoList ports;
            if (serialLoader.findPorts(PORT_PREFIX, true, ports) == 0) {
            }
            if (ports.size() == 0) {
                printf("error: no serial ports found\n");
                return 1;
            }
            info = ports.front();
            port = info.port();
        }
        if ((sts = serialLoader.connect(port, baudrate)) != 0) {
            printf("error: loader initialization failed: %d\n", sts);
            return 1;
        }
        loader = &serialLoader;
    }
    
    /* do an xbee download */
    else {
        XbeeAddr addr(0, 0);
        if (ipaddr) {
            uint32_t xbeeAddr;
            if (StringToAddr(ipaddr, &xbeeAddr) != 0) {
                printf("error: invalid IP address '%s'\n", ipaddr);
                return 1;
            }
            addr.set(0, xbeeAddr);
            if (addr.determineHostAddr() != 0) {
                printf("error: can't determine host IP address for '%s'\n", ipaddr);
                return 1;
            }
        }
        else {
            XbeeInfoList addrs;
            if (xbeeLoader.discover(false, addrs) != 0)
                printf("error: discover failed\n");
            if (addrs.size() == 0) {
                printf("error: no Xbee module found\n");
                return 1;
            }
            addr.set(addrs.front().hostAddr(), addrs.front().xbeeAddr());
        }
        
        if ((sts = xbeeLoader.connect(addr)) != 0) {
            printf("error: loader initialization failed: %d\n", sts);
            return 1;
        }
        
        loader = &xbeeLoader;
    }
    
    /* load the file */
    if ((sts = loader->loadFile(file)) != 0) {
        printf("error: load failed: %d\n", sts);
        return 1;
    }
    
    /* enter terminal mode */
    if (terminalMode) {
        printf("[ Entering terminal mode. Type ESC or Control-C to exit. ]\n");
        loader->setBaudRate(115200);
        loader->terminal(false, false);
    }
    
    /* disconnect from the target */
    loader->disconnect();
    
finish:
    /* return successfully */
    return 0;
}

void ShowPorts(const char *prefix, bool check)
{
    SerialLoader ldr;
    SerialInfoList ports;
    if (ldr.findPorts(prefix, check, ports) == 0) {
        SerialInfoList::iterator i = ports.begin();
        while (i != ports.end()) {
            std::cout << i->port() << std::endl;
            ++i;
        }
    }
}

void ShowXbeeModules(bool check)
{
    XbeeLoader ldr;
    XbeeInfoList addrs;
    if (ldr.discover(check, addrs) != 0)
        printf("Discover failed\n");
    else {
        XbeeInfoList::iterator i;
        for (i = addrs.begin(); i != addrs.end(); ++i) {
            printf("host:            %s\n", AddrToString(i->hostAddr()));
            printf("xbee:            %s\n", AddrToString(i->xbeeAddr()));
            printf("macAddrHigh:     %08x\n", i->macAddrHigh());
            printf("macAddrLow:      %08x\n", i->macAddrLow());
            printf("xbeePort:        %08x\n", i->xbeePort());
            printf("firmwareVersion: %08x\n", i->firmwareVersion());
            printf("cfgChecksum:     %08x\n", i->cfgChecksum());
            printf("nodeId:          '%s'\n", i->nodeID().c_str());
            printf("\n");
        }
    }
}

