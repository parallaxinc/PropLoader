#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xbee.hpp"
#include "sock.h"

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

Xbee::Xbee()
    : m_appService(INVALID_SOCKET), m_serialService(INVALID_SOCKET)
{
}

Xbee::~Xbee()
{
    disconnect();
}

int Xbee::connect(const char *ipaddr)
{
    int sts;
    
    if (m_appService != INVALID_SOCKET)
        return -1;
    
    if ((sts = ConnectSocket(ipaddr, DEF_APP_SERVICE_PORT, &m_appService)) != 0) {
        printf("error: failed to connect to app service on %s:%d\n", ipaddr, DEF_APP_SERVICE_PORT);
        return sts;
    }
    
    if ((sts = BindSocket(DEF_SERIAL_SERVICE_PORT, &m_serialService)) != 0) {
        printf("error: failed to bind to port %d\n", DEF_SERIAL_SERVICE_PORT);
        DisconnectSocket(m_appService);
        return sts;
    }
    
    return 0;
}

void Xbee::disconnect()
{
    if (m_appService != INVALID_SOCKET) {
        DisconnectSocket(m_appService);
        m_appService = INVALID_SOCKET;
    }
    if (m_serialService != INVALID_SOCKET) {
        DisconnectSocket(m_serialService);
        m_serialService = INVALID_SOCKET;
    }
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
    if (SendSocketData(m_appService, tx, sizeof(txPacket)) != sizeof(txPacket))
        return -1;
    if ((cnt = ReceiveSocketData(m_appService, buf, sizeof(buf))) < sizeof(rxPacket))
        return -1;
    if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00)
        return -1;
    *pValue = 0;
    for (i = sizeof(rxPacket); i < cnt; ++i)
        *pValue = (*pValue << 8) + buf[i];
    return 0;
}

int Xbee::setItem(xbCommand cmd, int value)
{
    uint8_t buf[2048];
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
    for (i = 0; i < sizeof(int); ++i)
        buf[sizeof(txPacket) + i] = value >> ((sizeof(int) - i - 1) * 8);
    if (SendSocketData(m_appService, tx, sizeof(txPacket) + sizeof(int)) != sizeof(txPacket) + sizeof(int))
        return -1;
    if ((cnt = ReceiveSocketData(m_appService, buf, sizeof(buf))) < sizeof(rxPacket))
        return -1;
    if ((rx->hdr.number1 ^ rx->hdr.number2) != 0x4242 || rx->status != 0x00)
        return -1;
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
