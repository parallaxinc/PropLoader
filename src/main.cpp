#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "loadelf.h"
#include "xbee-loader.hpp"
#include "serial-loader.hpp"

/* defaults */
//#define DEF_IPADDR              "10.0.1.88"
#define DEF_IPADDR              "10.0.1.3"
//#define DEF_PORT                "/dev/cu.usbserial-PAYMDDM"
#define DEF_PORT                "/dev/cu.usbserial-PAYMDDN"

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

void ShowPorts(const char *prefix);
void ShowConnectedPorts(const char *prefix, int baudrate, int verbose);

int main(int argc, char *argv[])
{
    bool ipaddr = false;
    XBEE_ADDR addr;
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
                //if (argv[i][2])
                //    ipaddr = &argv[i][2];
                //else if (++i < argc)
                //    ipaddr = argv[i];
                //else
                //    usage(argv[0]);
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
    
    printf("\nPorts:\n");
    ShowPorts(PORT_PREFIX);
    printf("\nConnected ports:\n");
    ShowConnectedPorts(PORT_PREFIX, baudrate, true);
    
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
        if (!ipaddr) {
            XBEE_ADDR addrs[10];
            int cnt;
            cnt = Xbee::discover(addrs, 10, 500);
            if (cnt < 0)
                printf("Discover failed: %d\n", cnt);
            else {
                for (i = 0; i < cnt; ++i) {
                    printf("host: %s\n", GetAddressStringX(addrs[i].host));
                    printf("xbee: %s\n", GetAddressStringX(addrs[i].xbee));
                }
            }
            if (cnt == 0) {
                printf("error: no Xbee module found\n");
                return 1;
            }
            addr = addrs[0];
        }
        
        if ((sts = xbeeLoader.init(&addr)) != 0) {
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

void *LoadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize)
{
    uint32_t start, imageSize, cogImagesSize;
    uint8_t *imageBuf, *buf;
    ElfContext *c;
    ElfProgramHdr program;
    int i;

    /* open the elf file */
    if (!(c = OpenElfFile(fp, hdr)))
        return NULL;
        
    /* get the total size of the program */
    if (!GetProgramSize(c, &start, &imageSize, &cogImagesSize)) {
        CloseElfFile(c);
        return NULL;
    }
        
    /* check to see if cog images in eeprom are allowed */
    if (cogImagesSize > 0) {
        CloseElfFile(c);
        return NULL;
    }
    
    /* allocate a buffer big enough for the entire image */
    if (!(imageBuf = (uint8_t *)malloc(imageSize)))
        return NULL;
    memset(imageBuf, 0, imageSize);
        
    /* load each program section */
    for (i = 0; i < c->hdr.phnum; ++i) {
        if (!LoadProgramTableEntry(c, i, &program)
        ||  !(buf = LoadProgramSegment(c, &program))) {
            CloseElfFile(c);
            free(imageBuf);
            return NULL;
        }
        if (program.paddr < COG_DRIVER_IMAGE_BASE)
            memcpy(&imageBuf[program.paddr - start], buf, program.filesz);
    }
    
    /* close the elf file */
    CloseElfFile(c);
    
    /* return the image */
    *pImageSize = imageSize;
    return (void *)imageBuf;
}

typedef struct {
    int baudrate;
    int verbose;
    char *actualport;
} CheckPortInfo;

static int ShowPort(const char *port, void *data)
{
    printf("%s\n", port);
    return 1;
}

void ShowPorts(const char *prefix)
{
    SerialFind(prefix, ShowPort, NULL);
}

static int ShowConnectedPort(const char *port, void *data)
{
    CheckPortInfo* info = (CheckPortInfo *)data;
    SerialLoader ldr;
    int version;
    
    if (info->verbose) {
        printf("Trying %s                    \r", port);
        fflush(stdout);
    }
    
    ldr.init(port, info->baudrate);
    if (ldr.identify(&version) == 0) {
        printf("%s, version %d\n", port, version);
        if (info->actualport) {
            strncpy(info->actualport, port, PATH_MAX - 1);
            info->actualport[PATH_MAX - 1] = '\0';
        }
    }
    
    return 1;
}

void ShowConnectedPorts(const char *prefix, int baudrate, int verbose)
{
    CheckPortInfo info;

    info.baudrate = baudrate;
    info.verbose = verbose;
    info.actualport = NULL;
    
    SerialFind(prefix, ShowConnectedPort, &info);
}
