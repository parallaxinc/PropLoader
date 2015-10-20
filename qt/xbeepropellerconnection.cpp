#include <netinet/in.h>
#include "xbeepropellerconnection.h"

#define APP_SERVICE_PORT    0xBEE
#define SERIAL_SERVICE_PORT 0x2616
#define XBEE_MAX_DATA_SIZE  1020

typedef enum {
    serialUDP = 0,
    serialTCP
} ipModes;

typedef enum {
    pinDisabled = 0,
    pinEnabled,
    pinAnalog,
    pinInput,
    pinOutLow,
    pinOutHigh,
} ioKinds;

typedef enum {
    transparentMode = 0,
    apiWoEscapeMode,
    apiWEscapeMode
} serialModes;

typedef enum {
    parityNone = 0,
    parityEven,
    parityOdd
} serialParity;

typedef enum {
    stopBits1 = 0,
    stopBits2
} stopBits;

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

XbeePropellerConnection::XbeePropellerConnection()
{
}

XbeePropellerConnection::XbeePropellerConnection(const QString & hostName, int baudRate)
{
    m_appService.connectToHost(hostName, APP_SERVICE_PORT);
    m_serialService.bind(SERIAL_SERVICE_PORT);
}

XbeePropellerConnection::~XbeePropellerConnection()
{
}

bool XbeePropellerConnection::isOpen()
{
}

int XbeePropellerConnection::close()
{
    m_appService.close();
    m_serialService.close();
    return 0;
}

int XbeePropellerConnection::generateResetSignal()
{
    if (enforceXbeeConfiguration() != 0)
        return -1;
    return setItem(xbOutputState, 0x0010);
}

int XbeePropellerConnection::sendData(const uint8_t *buf, int len)
{
    int packetSize = sizeof(appHeader) + len;
    QByteArray packet(packetSize, 0);
    appHeader *hdr;

    /* initialize the app header */
    hdr = (appHeader *)packet.data();
    hdr->number1 = 0x0000;
    hdr->number2 = hdr->number1 ^ 0x4242;
    hdr->packetID = 0;
    hdr->encryptionPad = 0;
    hdr->commandID = 0x00;
    hdr->commandOptions = 0x00;

    /* copy the data into the packet */
    packet.replace(sizeof(appHeader), len, (char *)buf, len);

    /* send the data to the xbee */
    return m_appService.write(packet);
}

void XbeePropellerConnection::pauseForVerification(int byteCount)
{
}

int XbeePropellerConnection::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    if (!m_appService.waitForReadyRead(timeout))
        return -1;
    return m_appService.read((char *)buf, len);
}

int XbeePropellerConnection::receiveDataExactTimeout(uint8_t *buf, int len, int timeout)
{
}

int XbeePropellerConnection::receiveChecksumAck(int byteCount, int delay)
{
}

int XbeePropellerConnection::setBaudRate(int baudRate)
{
    if (setItem(xbSerialBaud, baudRate) != 0)
        return -1;
    m_baudRate = baudRate;
    return 0;
}

int XbeePropellerConnection::maxDataSize()
{
    return XBEE_MAX_DATA_SIZE;
}

int XbeePropellerConnection::validate(xbCommand cmd, int value, bool readOnly)
{
    int currentValue;
    if (getItem(cmd, &currentValue) != 0)
        return -1;
    if (currentValue != value && !readOnly) {
        if (setItem(cmd, value) != 0)
            return -1;
    }
    return 0;
}

int XbeePropellerConnection::enforceXbeeConfiguration()
{
    if (validate(xbSerialIP, serialUDP, true) != 0              // Ensure XBee's Serial Service uses UDP packets [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbIPDestination, ntohl(m_addr), false) != 0    // Ensure Serial-to-IP destination is us (our IP)
    ||  validate(xbIPPort, SERIAL_SERVICE_PORT, false) != 0     // Ensure Serial-to-IP port is proper (default, in this case)
    ||  validate(xbOutputMask, 0x7FFF, false) != 0              // Ensure output mask is proper (default, in this case)
    ||  validate(xbRTSFlow, pinEnabled, false) != 0             // Ensure RTS flow pin is enabled (input)
    ||  validate(xbIO4Mode, pinOutLow, false) != 0              // Ensure serial hold pin is set to output low
    ||  validate(xbIO2Mode, pinOutHigh, false) != 0             // Ensure reset pin is set to output high
    ||  validate(xbIO4Timer, 2, false) != 0                     // Ensure serial hold pin's timer is set to 200 ms
    ||  validate(xbIO2Timer, 1, false) != 0                     // Ensure reset pin's timer is set to 100 ms
    ||  validate(xbSerialMode, transparentMode, true) != 0      // Ensure Serial Mode is transparent [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbSerialBaud, m_baudRate, false) != 0          // Ensure baud rate is set to initial speed
    ||  validate(xbSerialParity, parityNone, false) != 0        // Ensure parity is none
    ||  validate(xbSerialStopBits, stopBits1, false) != 0       // Ensure stop bits is 1
    ||  validate(xbPacketingTimeout, 3, false) != 0)            // Ensure packetization timout is 3 character times
        return -1;
    return 0;
}

int XbeePropellerConnection::getItem(xbCommand cmd, int *pValue)
{
    uint8_t txBuf[2048]; // BUG: get rid of this magic number!
    uint8_t rxBuf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)txBuf;
    rxPacket *rx = (rxPacket *)rxBuf;
    int cnt, i;

    if ((cnt = sendRemoteCommand(cmd, tx, sizeof(txPacket), rx, sizeof(rxBuf))) == -1)
        return -1;

    *pValue = 0;
    for (i = sizeof(rxPacket); i < cnt; ++i)
        *pValue = (*pValue << 8) + rxBuf[i];

    return 0;
}

int XbeePropellerConnection::getItem(xbCommand cmd, QString &value)
{
    uint8_t txBuf[2048]; // BUG: get rid of this magic number!
    uint8_t rxBuf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)txBuf;
    rxPacket *rx = (rxPacket *)rxBuf;
    int cnt;

    if ((cnt = sendRemoteCommand(cmd, tx, sizeof(txPacket), rx, sizeof(rxBuf))) == -1)
        return -1;

    rxBuf[cnt] = '\0'; // terminate the string
    value = QString((char *)&rxBuf[sizeof(rxPacket)]);

    return 0;
}

int XbeePropellerConnection::setItem(xbCommand cmd, int value)
{
    uint8_t txBuf[2048]; // BUG: get rid of this magic number!
    uint8_t rxBuf[2048]; // BUG: get rid of this magic number!
    txPacket *tx = (txPacket *)txBuf;
    rxPacket *rx = (rxPacket *)rxBuf;
    int cnt, i;

    for (i = 0; i < (int)sizeof(int); ++i)
        txBuf[sizeof(txPacket) + i] = value >> ((sizeof(int) - i - 1) * 8);

    if ((cnt = sendRemoteCommand(cmd, tx, sizeof(txPacket) + sizeof(int), rx, sizeof(rxBuf))) == -1)
        return -1;

    return 0;
}

int XbeePropellerConnection::sendRemoteCommand(xbCommand cmd, txPacket *tx, int txCnt, rxPacket *rx, int rxSize)
{
    int retries, timeout, cnt;

    tx->hdr.packetID = 0;
    tx->hdr.encryptionPad = 0;
    tx->hdr.commandID = 0x02;
    tx->hdr.commandOptions = 0x00;
    tx->frameID = 0x01;
    tx->configOptions = 0x02;
    strncpy(tx->atCommand, atCmd[cmd], 2);

    retries = 3;
    timeout = 500;
    while (--retries >= 0) {
        uint16_t number = rand();

        tx->hdr.number1 = number;
        tx->hdr.number2 = tx->hdr.number1 ^ 0x4242;

        if (m_appService.write((char *)tx, txCnt) != txCnt)
            return -1;

        if (m_appService.waitForReadyRead(timeout)) {
            if ((cnt = m_appService.read((char *)rx, rxSize)) >= (int)sizeof(rxPacket)
            &&  (rx->hdr.number1 ^ rx->hdr.number2) == 0x4242
            &&  rx->hdr.number1 == number
            &&  rx->status == 0x00)
                return cnt;
        }
    }

    return -1;
}

