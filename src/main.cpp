#include <stdio.h>
#include <stdlib.h>
#include "xbee-loader.hpp"
#include "serial-loader.hpp"

/* defaults */
#define DEF_IPADDR              "10.0.1.88"

static void usage(const char *progname)
{
printf("\
usage: %s\n\
         [ -b <baudrate> ]  set the initial baud rate\n\
         [ -i <ip-addr> ]   set the IP address of the Xbee Wi-Fi module\n\
         [ -? ]             display a usage message and exit\n\
         <file>             spin binary file to load\n", progname);
    exit(1);
}

int main(int argc, char *argv[])
{
    const char *ipaddr = DEF_IPADDR;
    int baudrate = DEFAULT_BAUDRATE;
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

#if 0
    /* initialize the loader */
    if ((sts = xbeeLoader.init(ipaddr)) != 0) {
        printf("error: loader initialization failed: %d\n", sts);
        return 1;
    }
    loader = &xbeeLoader;
#endif
    
    /* initialize the loader */
    if ((sts = serialLoader.init("/dev/cu.usbserial-PAYMDDM", baudrate)) != 0) {
        printf("error: loader initialization failed: %d\n", sts);
        return 1;
    }
    loader = &serialLoader;
        
    /* load the file */
    if ((sts = loader->loadFile(file)) != 0) {
        printf("error: load failed: %d\n", sts);
        return 1;
    }
    
    /* return successfully */
    return 0;
}
