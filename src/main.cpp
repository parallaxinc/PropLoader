#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include <iostream>

#include "loadelf.h"
#include "propimage.h"
#include "packet.h"
#include "loader.h"
#include "serialpropconnection.h"
#include "wifipropconnection.h"
#include "config.h"

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
         [ -e ]             program eeprom\n\
         [ -i <ip-addr> ]   IP address of the Xbee Wi-Fi module\n\
         [ -n <name> ]      set the friendly name of an Xbee Wi-Fi module\n\
         [ -p <port> ]      serial port\n\
         [ -P ]             show all serial ports with propellers connected\n\
         [ -P0 ]            show all serial ports\n\
         [ -r ]             run program after downloading\n\
         [ -R ]             reset the Propeller\n\
         [ -s ]             do a serial download\n\
         [ -t ]             enter terminal mode after the load is complete\n\
         [ -W ]             show all discovered wifi modules with propellers connected\n\
         [ -W0 ]            show all discovered wifi modules\n\
         [ -? ]             display a usage message and exit\n\
         <file>             spin binary file to load\n", progname, DEFAULT_BAUDRATE);
    exit(1);
}

static void ShowPorts(const char *prefix, bool check);
static void ShowWiFiModules(bool check);
static void SetFriendlyName(const char *ipaddr, const char *name);

int main(int argc, char *argv[])
{
    bool done = false;
    bool reset = false;
    bool terminalMode = false;
    const char *ipaddr = NULL;
    const char *port = NULL;
    const char *name = NULL;
    int baudrate = DEFAULT_BAUDRATE;
    int loadType = ltShutdown;
    bool useSerial = false;
    char *file = NULL;
    PropConnection *connection;
    Loader loader;
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
            case 'e':
                loadType |= ltDownloadAndProgram;
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
            case 'n':
                if (argv[i][2])
                    name = &argv[i][2];
                else if (++i < argc)
                    name = argv[i];
                else
                    usage(argv[0]);
                SetFriendlyName(ipaddr, name);
                done = true;
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
            case 'r':
                loadType |= ltDownloadAndRun;
                break;
            case 'R':
                reset = true;
                done = true;
                break;
            case 's':
                useSerial = true;
                break;
            case 't':
                terminalMode = true;
                break;
            case 'W':
                ShowWiFiModules(argv[i][2] == '0' ? false : true);
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
    if (!done && !reset && !file && !terminalMode)
        usage(argv[0]);
        
    /* check to see if a reset was requested or there is a file to load */
    if (!reset && !file)
        goto finish;

    /* default to 'download and run' if neither -e nor -r are specified */
    if (loadType == ltShutdown)
        loadType = ltDownloadAndRun;
        
    /* do a serial download */
    if (useSerial) {
        SerialPropConnection *serialConnection;
        SerialInfo info; // needs to stay in scope as long as we're using port
        if (!(serialConnection = new SerialPropConnection)) {
            printf("error: insufficient memory\n");
            return 1;
        }
        if (!port) {
            SerialInfoList ports;
            if (SerialPropConnection::findPorts(PORT_PREFIX, true, ports) != 0) {
                printf("error: serial port discovery failed\n");
                return 1;
            }
            if (ports.size() == 0) {
                printf("error: no serial ports found\n");
                return 1;
            }
            info = ports.front();
            port = info.port();
        }
        if ((sts = serialConnection->open(port, baudrate)) != 0) {
            printf("error: loader initialization failed: %d\n", sts);
            return 1;
        }
        connection = serialConnection;
    }
    
    /* do a wifi download */
    else {
        WiFiPropConnection *wifiConnection;
        if (!(wifiConnection = new WiFiPropConnection)) {
            printf("error: insufficient memory\n");
            return 1;
        }
        if (!ipaddr) {
#if 0
            XbeeInfoList addrs;
            if (xbeeLoader.discover(false, addrs) != 0)
                printf("error: discover failed\n");
            if (addrs.size() == 0) {
                printf("error: no Xbee module found\n");
                return 1;
            }
            addr.set(addrs.front().hostAddr(), addrs.front().xbeeAddr());
#endif
        }
        if ((sts = wifiConnection->setAddress(ipaddr)) != 0) {
            printf("error: setAddress failed: %d\n", sts);
            return 1;
        }
        connection = wifiConnection;
    }
    
    if (reset)
        connection->generateResetSignal();
    
    /* load the file */
    loader.setConnection(connection);
    if (file && (sts = loader.loadFile(file, (LoadType)loadType)) != 0) {
        printf("error: load failed: %d\n", sts);
        return 1;
    }
    
    /* enter terminal mode */
    if (terminalMode) {
        printf("[ Entering terminal mode. Type ESC or Control-C to exit. ]\n");
        connection->setBaudRate(115200);
        connection->terminal(false, false);
    }
    
    /* disconnect from the target */
    connection->disconnect();
    
finish:
    /* return successfully */
    return 0;
}

static void ShowPorts(const char *prefix, bool check)
{
    SerialInfoList ports;
    if (SerialPropConnection::findPorts(prefix, check, ports) == 0) {
        SerialInfoList::iterator i = ports.begin();
        while (i != ports.end()) {
            std::cout << i->port() << std::endl;
            ++i;
        }
    }
}

static void ShowWiFiModules(bool check)
{
    WiFiInfoList list;
    WiFiPropConnection::findModules(PORT_PREFIX, false, list);
}

static void SetFriendlyName(const char *ipaddr, const char *name)
{
}

int Error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    return -1;
}

extern "C" {
    extern uint8_t sd_helper_array[];
    extern int sd_helper_size;
}

/* DAT header in sd_helper.spin */
typedef struct {
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint8_t tvpin;
    uint8_t dopin;
    uint8_t clkpin;
    uint8_t dipin;
    uint8_t cspin;
    uint8_t select_address;
    uint32_t select_inc_mask;
    uint32_t select_mask;
} SDHelperDatHdr;

int LoadSDHelper(BoardConfig *config, PropConnection *connection)
{
    Loader loader(connection);
    PropImage image(sd_helper_array, sd_helper_size);
    SpinHdr *hdr = (SpinHdr *)image.imageData();
    SpinObj *obj = (SpinObj *)(image.imageData() + hdr->pbase);
    SDHelperDatHdr *dat = (SDHelperDatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    int ivalue;

    /* patch SD helper */
    if (GetNumericConfigField(config, "clkfreq", &ivalue))
        hdr->clkfreq = ivalue;
    if (GetNumericConfigField(config, "clkmode", &ivalue))
        hdr->clkmode = ivalue;
    if (GetNumericConfigField(config, "baudrate", &ivalue))
        dat->baudrate = ivalue;
    if (GetNumericConfigField(config, "rxpin", &ivalue))
        dat->rxpin = ivalue;
    if (GetNumericConfigField(config, "txpin", &ivalue))
        dat->txpin = ivalue;
    if (GetNumericConfigField(config, "tvpin", &ivalue))
        dat->tvpin = ivalue;

    if (GetNumericConfigField(config, "sdspi-do", &ivalue))
        dat->dopin = ivalue;
    else
        return Error("missing sdspi-do pin configuration");

    if (GetNumericConfigField(config, "sdspi-clk", &ivalue))
        dat->clkpin = ivalue;
    else
        return Error("missing sdspi-clk pin configuration");

    if (GetNumericConfigField(config, "sdspi-di", &ivalue))
        dat->dipin = ivalue;
    else
        return Error("missing sdspi-di pin configuration");

    if (GetNumericConfigField(config, "sdspi-cs", &ivalue))
        dat->cspin = ivalue;
    else if (GetNumericConfigField(config, "sdspi-clr", &ivalue))
        dat->cspin = ivalue;
    else
        return Error("missing sdspi-cs or sdspi-clr pin configuration");

    if (GetNumericConfigField(config, "sdspi-sel", &ivalue))
        dat->select_inc_mask = ivalue;
    else if (GetNumericConfigField(config, "sdspi-inc", &ivalue))
        dat->select_inc_mask = 1 << ivalue;

    if (GetNumericConfigField(config, "sdspi-msk", &ivalue))
        dat->select_mask = ivalue;

    if (GetNumericConfigField(config, "sdspi-addr", &ivalue))
        dat->select_address = (uint8_t)ivalue;

    /* recompute the checksum */
    image.updateChecksum();

    /* load the SD helper program */
    if (loader.fastLoadImage(image.imageData(), image.imageSize(), ltDownloadAndRun) != 0)
        return Error("helper load failed");

    return 0;
}

#define TYPE_FILE_WRITE     0
#define TYPE_DATA           1
#define TYPE_EOF            2

int WriteFileToSDCard(BoardConfig *config, PropConnection *connection, const char *path, const char *target)
{
    PacketDriver packetDriver(*connection);
    uint8_t buf[PKTMAXLEN];
    size_t size, remaining, cnt;
    FILE *fp;

    /* open the file */
    printf("Opening '%s'\n", path);
    if ((fp = fopen(path, "rb")) == NULL)
        return Error("can't open %s", path);

    if (!target) {
        if (!(target = strrchr(path, '/')))
            target = path;
        else
            ++target; // skip past the slash
    }
    printf("Target is '%s'\n", target);

    fseek(fp, 0, SEEK_END);
    size = remaining = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("Loading SD helper\n");
    if (LoadSDHelper(config, connection) != 0) {
        fclose(fp);
        return Error("loading SD helper");
    }

    /* switch to the final baud rate */
    int baudRate = 115200;
//    GetNumericConfigField(config, "baudrate", &baudRate);
    printf("Switching to %d baud\n", baudRate);
    if (connection->setBaudRate(baudRate) != 0) {
        printf("error: setting baud rate failed\n");
        return -1;
    }

    /* wait for the SD helper to complete initialization */
    printf("Waiting for SD helper to start\n");
    if (!packetDriver.waitForInitialAck())
        return Error("failed to connect to helper");

    printf("Starting file write\n");
    if (!packetDriver.sendPacket(TYPE_FILE_WRITE, (uint8_t *)target, strlen(target) + 1)) {
        fclose(fp);
        return Error("SendPacket FILE_WRITE failed");
    }

    printf("Loading '%s' to SD card\n", path);
    while ((cnt = fread(buf, 1, PKTMAXLEN, fp)) > 0) {
        printf("%ld bytes remaining             \r", remaining); fflush(stdout);
        if (!packetDriver.sendPacket(TYPE_DATA, buf, cnt)) {
            fclose(fp);
            return Error("SendPacket DATA failed");
        }
        remaining -= cnt;
    }
    printf("%ld bytes sent             \n", size);

    fclose(fp);

    if (!packetDriver.sendPacket(TYPE_EOF, (uint8_t *)"", 0))
        return Error("SendPacket EOF failed");

    /*
       We send two EOF packets for SD card writes.  The reason is that the EOF
       packet does actual work, and that work takes time.  The packet
       transmission protocol uses read-ahead buffering on the receiving end.
       Therefore, we need to make sure the first EOF packet was received and
       processed before resetting the Prop!
    */
    if (!packetDriver.sendPacket(TYPE_EOF, (uint8_t *)"", 0))
        return Error("Second SendPacket EOF failed");

    return 0;
}

