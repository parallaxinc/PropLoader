#include <stdio.h>
#include <stdlib.h>
#include "xbee-loader.hpp"
#include "serial-loader.hpp"

/* defaults */
#define DEF_IPADDR              "10.0.1.88"
#define DEF_PORT                "/dev/cu.usbserial-PAYMDDM"

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
         [ -b <baudrate> ]  set the initial baud rate (default is %d)\n\
         [ -i <ip-addr> ]   set the IP address of the Xbee Wi-Fi module (default is %s)\n\
         [ -p <port> ]      serial port (default is %s)\n\
         [ -s ]             do a serial download\n\
         [ -x ]             use the single stage loader (for testing)\n\
         [ -? ]             display a usage message and exit\n\
         <file>             spin binary file to load\n", progname, DEFAULT_BAUDRATE, DEF_IPADDR, DEF_PORT);
    exit(1);
}

int main(int argc, char *argv[])
{
    const char *ipaddr = DEF_IPADDR;
    const char *port = DEF_PORT;
    int baudrate = DEFAULT_BAUDRATE;
    bool useSerial = false;
    bool useSingleStageLoader = false;
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
                break;
            case 's':
                useSerial = true;
                break;
            case 'x':
                useSingleStageLoader = true;
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
    if (!file)
        usage(argv[0]);

    /* do a serial download */
    if (useSerial) {
        if ((sts = serialLoader.init(port, baudrate)) != 0) {
            printf("error: loader initialization failed: %d\n", sts);
            return 1;
        }
        loader = &serialLoader;
    }
    
    /* do an xbee download */
    else {
        if ((sts = xbeeLoader.init(ipaddr)) != 0) {
            printf("error: loader initialization failed: %d\n", sts);
            return 1;
        }
        loader = &xbeeLoader;
    }
    
    /* load the file */
    if ((sts = useSingleStageLoader ? loader->loadFile(file) : loader->loadFile2(file)) != 0) {
        printf("error: load failed: %d\n", sts);
        return 1;
    }
    
    /* return successfully */
    return 0;
}
