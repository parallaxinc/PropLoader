#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xbee.hpp"
#include "sock.h"

#define MAX_IF_ADDRS    10

// Define XBee WiFi's AT commands.
static const char *atCmd[] = {
    // NOTES: [R] - read only, [R/W] = read/write, [s] - string, [b] - binary number, [sb] - string or binary number
    /* xbData */              "\0\0",/* [Wb] write data stream */
    /* xbMacHigh */           "SH",  /* [Rb] XBee's Mac Address (highest 16-bits) */
    /* xbMacLow */            "SL",  /* [Rb] XBee's Mac Address (lowest 32-bits) */
    /* xbSSID */              "ID",  /* [Rs/Ws] SSID (0 to 31 printable ASCII characters) */
    /* xbIPAddr */            "MY",  /* [Rb/Wsb*] XBee's IP Address (32-bits; IPv4); *Read-only in DHCP mode */
    /* xbIPMask */            "MK",  /* [Rb/Wsb*] XBee's IP Mask (32-bits); *Read-only in DHCP mode */
    /* xbIPGateway */         "GW",  /* [Rb/Wsb*] XBee's IP Gateway (32-bits); *Read-only in DHCP mode */
    /* xbIPPort */            "C0",  /* [Rb/Wb] Xbee's UDP/IP Port (16-bits) */
    /* xbIPDestination */     "DL",  /* [Rb/Wsb] Xbee's serial-to-IP destination address (32-bits; IPv4) */
    /* xbNodeID */            "NI",  /* [Rs/Ws] Friendly node identifier string (20 printable ASCII characters) */
    /* xbMaxRFPayload */      "NP",  /* [Rb] Maximum RF Payload (16-bits; in bytes) */
    /* xbPacketingTimeout */  "RO",  /* [Rb/Wb] Inter-character silence time that triggers packetization (8-bits; in character times) */
    /* xbIO2Mode */           "D2",  /* [Rb/Wb] Designated reset pin (3-bits; 0=Disabled, 1=SPI_CLK, 2=Analog input, 3=Digital input, 4=Digital output low, 5=Digital output high) */
    /* xbIO4Mode */           "D4",  /* [Rb/Wb] Designated serial hold pin (3-bits; 0=Disabled, 1=SPI_MOSI, 2=<undefined>, 3=Digital input, 4=Digital output low, 5=Digital output high) */
    /* xbOutputMask */        "OM",  /* [Rb/Wb] Output mask for all I/O pins (each 1=output pin, each 0=input pin) (15-bits on TH, 20-bits on SMT) */
    /* xbOutputState */       "IO",  /* [Rb/Wb] Output state for all I/O pins (each 1=high, each 0=low) (15-bits on TH, 20-bits on SMT).  Period affected by updIO2Timer */
    /* xbIO2Timer */          "T2",  /* [Rb/Wb] I/O 2 state timer (100 ms units; $0..$1770) */
    /* xbIO4Timer */          "T4",  /* [Rb/Wb] I/O 4 state timer (100 ms units; $0..$1770) */
    /* xbSerialMode */        "AP",  /* [Rb/Wb] Serial mode (0=Transparent, 1=API wo/Escapes, 2=API w/Escapes) */
    /* xbSerialBaud */        "BD",  /* [Rb/Wb] serial baud rate ($1=2400, $2=4800, $3=9600, $4=19200, $5=38400, $6=57600, $7=115200, $8=230400, $9=460800, $A=921600) */
    /* xbSerialParity */      "NB",  /* [Rb/Wb] serial parity ($0=none, $1=even, $2=odd) */
    /* xbSerialStopBits */    "SB",  /* [Rb/Wb] serial stop bits ($0=one stop bit, $1=two stop bits) */
    /* xbRTSFlow */           "D6",  /* [Rb/Wb] RTS flow control pin (3-bits; 0=Disabled, 1=RTS Flow Control, 2=<undefined>, 3=Digital input, 4=Digital output low, 5=Digital output high) */
    /* xbSerialIP */          "IP",  /* [Rb/Wb] Protocol for serial service (0=UDP, 1=TCP) */
    /* xbFirmwareVer */       "VR",  /* [Rb] Firmware version.  Nibbles ABCD; ABC = major release, D = minor release.  B = 0 (standard release), B > 0 (variant release) (16-bits) */
    /* xbHardwareVer */       "HV",  /* [Rb] Hardware version.  Nibbles ABCD; AB = module type, CD = revision (16-bits) */
    /* xbHardwareSeries */    "HS",  /* [Rb] Hardware series. (16-bits?) */
    /* xbChecksum */          "CK"   /* [Rb] current configuration checksum (16-bits) */
};

int XbeeAddr::determineHostAddr()
{
    IFADDR ifaddrs[MAX_IF_ADDRS];
    int cnt, i;
    
    if ((cnt = GetInterfaceAddresses(ifaddrs, MAX_IF_ADDRS)) < 0)
        return -1;
    
    for (i = 0; i < cnt; ++i) {
        uint32_t hostAddr = ifaddrs[i].addr.sin_addr.s_addr;
        uint32_t mask = ifaddrs[i].mask.sin_addr.s_addr;
        if ((m_xbeeAddr & mask) == (hostAddr & mask)) {
            m_hostAddr = hostAddr;
            return 0;
        } 
    }

    return -1;
}

Xbee::Xbee()
    : m_appService(INVALID_SOCKET),
      m_serialService(INVALID_SOCKET)
{
}

Xbee::~Xbee()
{
    disconnect();
}

int Xbee::discover(XbeeAddrList &addrs, int timeout)
{
    IFADDR ifaddrs[MAX_IF_ADDRS];
    int cnt, i;
    
    if ((cnt = GetInterfaceAddresses(ifaddrs, MAX_IF_ADDRS)) < 0)
        return -1;
    printf("Found %d interfaces\n", cnt);
    
    for (i = 0; i < cnt; ++i)
        discover1(&ifaddrs[i], addrs, timeout);
    
    return 0;
}

int Xbee::discover1(IFADDR *ifaddr, XbeeAddrList &addrs, int timeout)
{
    uint8_t buf[2048]; // BUG: get rid of this magic number!
    int retries = DEF_DISCOVER_RETRIES;
    txPacket *tx = (txPacket *)buf;
    rxPacket *rx = (rxPacket *)buf;
    SOCKADDR_IN bcastaddr;
    SOCKET sock;
    int cnt, i;
    
    printf("Checking %s\n", inet_ntoa(ifaddr->addr.sin_addr));
        
    /* create a broadcast socket */
    if (OpenBroadcastSocket(&sock) != 0)
        return -2;
        
    /* build a broadcast packet */
    tx->hdr.number1 = 0x0000;
    tx->hdr.number2 = tx->hdr.number1 ^ 0x4242;
    tx->hdr.packetID = 0;
    tx->hdr.encryptionPad = 0;
    tx->hdr.commandID = 0x02;
    tx->hdr.commandOptions = 0x00;
    tx->frameID = 0x01;
    tx->configOptions = 0x02;
    strncpy(tx->atCommand, "MY", 2);
    
    /* build a broadcast address */
    bcastaddr = ifaddr->bcast;
    bcastaddr.sin_port = htons(DEF_APP_SERVICE_PORT);
    
    /* send the broadcast packets */
    while (--retries >= 0) {
        if (SendSocketDataTo(sock, tx, sizeof(txPacket), &bcastaddr) != sizeof(txPacket)) {
            CloseSocket(sock);
            return -1;
        }
    }

    /* receive Xbee responses */
    while (SocketDataAvailableP(sock, timeout)) {

        /* get the next response */
        if ((cnt = ReceiveSocketData(sock, buf, sizeof(buf))) < (int)sizeof(rxPacket)) {
            CloseSocket(sock);
            return -1;
        }
        printf("Got one!\n");

        /* verify the packet header */
        if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00) {
            CloseSocket(sock);
            return -1;
        }

        /* store the IP addresses of the host and the Xbee module */
        uint32_t hostAddr = ((SOCKADDR_IN *)&ifaddr->addr)->sin_addr.s_addr;
        uint32_t xbeeAddr = 0;
        for (i = sizeof(rxPacket); i < cnt; ++i)
            xbeeAddr = (xbeeAddr << 8) + buf[i];
        XbeeAddr addr(hostAddr, htonl(xbeeAddr));
        addrs.push_back(addr);
    }
    
    /* get rid of duplicate entries */
    addrs.sort();
    addrs.unique();
    printf("%d unique\n", addrs.size());
    
    /* close the socket */
    CloseSocket(sock);
    
    /* return successfully */
    return 0;
}

int Xbee::connect(uint32_t addr)
{
    int sts;
    
    if (m_appService != INVALID_SOCKET)
        return -1;
    
    if ((sts = ConnectSocket(addr, DEF_APP_SERVICE_PORT, &m_appService)) != 0) {
        IN_ADDR inaddr;
        inaddr.s_addr = addr;
        printf("error: failed to connect to app service on %s:%d\n", inet_ntoa(inaddr), DEF_APP_SERVICE_PORT);
        return sts;
    }
    
    if ((sts = BindSocket(DEF_SERIAL_SERVICE_PORT, &m_serialService)) != 0) {
        printf("error: failed to bind to port %d\n", DEF_SERIAL_SERVICE_PORT);
        CloseSocket(m_appService);
        return sts;
    }
    
    return 0;
}

void Xbee::disconnect()
{
    if (m_appService != INVALID_SOCKET) {
        CloseSocket(m_appService);
        m_appService = INVALID_SOCKET;
    }
    if (m_serialService != INVALID_SOCKET) {
        CloseSocket(m_serialService);
        m_serialService = INVALID_SOCKET;
    }
}

void Xbee::terminal(bool checkForExit, bool pstMode)
{
    SocketTerminal(m_serialService, checkForExit, pstMode);
}

int Xbee::getItem(xbCommand cmd, int *pValue)
{
    uint8_t buf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)buf;
    rxPacket *rx = (rxPacket *)buf;
    int cnt, i;
    
    tx->hdr.number1 = 0x0000;
    tx->hdr.number2 = tx->hdr.number1 ^ 0x4242;
    tx->hdr.packetID = 0;
    tx->hdr.encryptionPad = 0;
    tx->hdr.commandID = 0x02;
    tx->hdr.commandOptions = 0x00;
    tx->frameID = 0x01;
    tx->configOptions = 0x02;
    strncpy(tx->atCommand, atCmd[cmd], 2);
    //printf("getItem %s --> ", atCmd[cmd]); fflush(stdout);
    if (SendSocketData(m_appService, tx, sizeof(txPacket)) != sizeof(txPacket))
        return -1;
    if ((cnt = ReceiveSocketData(m_appService, buf, sizeof(buf))) < (int)sizeof(rxPacket))
        return -1;
    if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00)
        return -1;
    for (i = sizeof(rxPacket); i < cnt; ++i)
        *pValue = (*pValue << 8) + buf[i];
    //printf("%d (%08x)\n", *pValue, *pValue);
    return 0;
}

int Xbee::getItem(xbCommand cmd, std::string &value)
{
    uint8_t buf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)buf;
    rxPacket *rx = (rxPacket *)buf;
    int cnt;
    
    tx->hdr.number1 = 0x0000;
    tx->hdr.number2 = tx->hdr.number1 ^ 0x4242;
    tx->hdr.packetID = 0;
    tx->hdr.encryptionPad = 0;
    tx->hdr.commandID = 0x02;
    tx->hdr.commandOptions = 0x00;
    tx->frameID = 0x01;
    tx->configOptions = 0x02;
    strncpy(tx->atCommand, atCmd[cmd], 2);
    //printf("getItem %s --> ", atCmd[cmd]); fflush(stdout);
    if (SendSocketData(m_appService, tx, sizeof(txPacket)) != sizeof(txPacket))
        return -1;
    if ((cnt = ReceiveSocketData(m_appService, buf, sizeof(buf))) < (int)sizeof(rxPacket))
        return -1;
    if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00)
        return -1;
    buf[cnt] = '\0'; // terminate the string
    value = std::string((char *)&buf[sizeof(rxPacket)]);
    //printf("'%s'\n", &buf[sizeof(rxPacket)]);
    return 0;
}

int Xbee::setItem(xbCommand cmd, int value)
{
    uint8_t buf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)buf;
    rxPacket *rx = (rxPacket *)buf;
    int cnt, i;
    
    tx->hdr.number1 = 0x0000;
    tx->hdr.number2 = tx->hdr.number1 ^ 0x4242;
    tx->hdr.packetID = 0;
    tx->hdr.encryptionPad = 0;
    tx->hdr.commandID = 0x02;
    tx->hdr.commandOptions = 0x00;
    tx->frameID = 0x01;
    tx->configOptions = 0x02;
    strncpy(tx->atCommand, atCmd[cmd], 2);
    for (i = 0; i < (int)sizeof(int); ++i)
        buf[sizeof(txPacket) + i] = value >> ((sizeof(int) - i - 1) * 8);
    //printf("setItem %s to %d (%08x) --> ", atCmd[cmd], value, value); fflush(stdout);
    if (SendSocketData(m_appService, tx, sizeof(txPacket) + sizeof(int)) != sizeof(txPacket) + sizeof(int))
        return -1;
    if ((cnt = ReceiveSocketData(m_appService, buf, sizeof(buf))) < (int)sizeof(rxPacket))
        return -1;
    if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00)
        return -1;
    //printf("done\n");
    return 0;
}

int Xbee::sendAppData(void *buf, int len)
{
    return SendSocketData(m_appService, buf, len);
}

int Xbee::receiveAppData(void *buf, int len)
{
    return ReceiveSocketData(m_appService, buf, len);
}

int Xbee::receiveSerialData(void *buf, int len)
{
    return ReceiveSocketData(m_serialService, buf, len);
}

int Xbee::receiveSerialDataTimeout(void *buf, int len, int timeout)
{
    return ReceiveSocketData(m_serialService, buf, len);
}
