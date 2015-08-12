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
            case 'x':
                useSingleStageLoader = true;
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
#if 0
                SerialInfoList::iterator i = ports.begin();
                while (i != ports.end()) {
                    std::cout << i->port() << std::endl;
                    ++i;
                }
#endif
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
                printf("Discover failed\n");
            else {
#if 0
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
#endif
            }
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
    
    /* open the binary */
    FILE *fp;
    if (!(fp = fopen(file, "rb"))) {
        printf("error: can't open '%s'\n", file);
        return 1;
    }
    
    /* check for an elf file */
    uint8_t *image;
    int imageSize;
    ElfHdr elfHdr;
    if (ReadAndCheckElfHdr(fp, &elfHdr)) {
        printf("Loading ELF file '%s'\n", file);
        image = LoadElfFile(fp, &elfHdr, &imageSize);
    }
    else {
        printf("Loading binary file '%s'\n", file);
        image = LoadSpinBinaryFile(fp, &imageSize);
        fclose(fp);
    }
    
    /* load the file */
    if ((sts = useSingleStageLoader ? loader->loadTinyImage(image, imageSize) : loader->loadImage(image, imageSize)) != 0) {
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

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

uint8_t *LoadSpinBinaryFile(FILE *fp, int *pLength)
{
    uint8_t *image;
    int imageSize;
    
    /* get the size of the binary file */
    fseek(fp, 0, SEEK_END);
    imageSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* allocate space for the file */
    if (!(image = (uint8_t *)malloc(imageSize)))
        return NULL;
    
    /* read the entire image into memory */
    if ((int)fread(image, 1, imageSize, fp) != imageSize) {
        free(image);
        return NULL;
    }
    
    /* return the buffer containing the file contents */
    *pLength = imageSize;
    return image;
}

/* spin object file header */
typedef struct {
    uint32_t clkfreq;
    uint8_t clkmode;
    uint8_t chksum;
    uint16_t pbase;
    uint16_t vbase;
    uint16_t dbase;
    uint16_t pcurr;
    uint16_t dcurr;
} SpinHdr;

uint8_t *LoadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize)
{
    uint32_t start, imageSize, cogImagesSize;
    uint8_t *image, *buf;
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
    if (!(image = (uint8_t *)malloc(imageSize)))
        return NULL;
    memset(image, 0, imageSize);
        
    /* load each program section */
    for (i = 0; i < c->hdr.phnum; ++i) {
        if (!LoadProgramTableEntry(c, i, &program)
        ||  !(buf = LoadProgramSegment(c, &program))) {
            CloseElfFile(c);
            free(image);
            return NULL;
        }
        if (program.paddr < COG_DRIVER_IMAGE_BASE)
            memcpy(&image[program.paddr - start], buf, program.filesz);
    }
    
    /* close the elf file */
    CloseElfFile(c);
    
    SpinHdr *spinHdr = (SpinHdr *)image;
    uint8_t *p = image;
    int cnt = imageSize;
    int chksum = 0;

    /* fixup the spin binary header */
    spinHdr->vbase = imageSize;
    spinHdr->dbase = imageSize + 2 * sizeof(uint32_t); // stack markers
    spinHdr->dcurr = spinHdr->dbase + sizeof(uint32_t);

    /* update the checksum */
    spinHdr->chksum = 0;
    while (--cnt >= 0)
        chksum += *p++;
    spinHdr->chksum = SPIN_TARGET_CHECKSUM - chksum;

    /* return the image */
    *pImageSize = imageSize;
    return image;
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

