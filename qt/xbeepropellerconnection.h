#ifndef XBEEPROPELLERCONNECTION_H
#define XBEEPROPELLERCONNECTION_H

#include <QUdpSocket>
#include "propellerconnection.h"

#define INITIAL_BAUD_RATE   115200
#define FINAL_BAUD_RATE     921600

typedef enum {
    xbData,
    xbMacHigh,
    xbMacLow,
    xbSSID,
    xbIPAddr,
    xbIPMask,
    xbIPGateway,
    xbIPPort,
    xbIPDestination,
    xbNodeID,
    xbMaxRFPayload,
    xbPacketingTimeout,
    xbIO2Mode,
    xbIO4Mode,
    xbOutputMask,
    xbOutputState,
    xbIO2Timer,
    xbIO4Timer,
    xbSerialMode,
    xbSerialBaud,
    xbSerialParity,
    xbSerialStopBits,
    xbRTSFlow,
    xbSerialIP,
    xbFirmwareVer,
    xbHardwareVer,
    xbHardwareSeries,
    xbChecksum
} xbCommand;

typedef struct {
    uint16_t number1;		    // Can be any random number.
    uint16_t number2;			// Must be number1 ^ 0x4242
    uint8_t packetID;			// Reserved (use 0)
    uint8_t encryptionPad;		// Reserved (use 0)
    uint8_t commandID;			// $00 = Data, $02 = Remote Command, $03 = General Purpose Memory Command, $04 = I/O Sample
    uint8_t commandOptions;     // Bit 0 : Encrypt (Reserved), Bit 1 : Request Packet ACK, Bits 2..7 : (Reserved)
} appHeader;

typedef struct {
    appHeader hdr;              // application packet header
    uint8_t frameID;			// 1
    uint8_t configOptions;		// 0 = Queue command only; must follow with AC command to apply changes, 2 = Apply Changes immediately
    char atCommand[2];		    // Command Name - Two ASCII characters that identify the AT command
} txPacket;

typedef struct {
    appHeader hdr;              // application packet header
    uint8_t frameID;			// 1
    char atCommand[2];		    // Command Name - Two ASCII characters that identify the AT command
    uint8_t status;             // Command status (0 = OK, 1 = ERROR, 2 = Invalid Command, 3 = Invalid Parameter)
} rxPacket;

class XbeePropellerConnection : public PropellerConnection
{
public:
    XbeePropellerConnection();
    XbeePropellerConnection(const QString &hostName, int baudRate = INITIAL_BAUD_RATE);
    ~XbeePropellerConnection();
    int open(const QString &hostName, int baudRate = INITIAL_BAUD_RATE);
    bool isOpen();
    int close();
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int receiveChecksumAck(int byteCount, int delay);
    int setBaudRate(int baudRate);
    int maxDataSize();

private:
    int validate(xbCommand cmd, int value, bool readOnly);
    int enforceXbeeConfiguration();
    int getItem(xbCommand cmd, int *pValue);
    int getItem(xbCommand cmd, QString &value);
    int setItem(xbCommand cmd, int value);
    int sendRemoteCommand(xbCommand cmd, txPacket *tx, int txCnt, rxPacket *rx, int rxSize);

    uint32_t m_addr;
    uint32_t m_hostAddr;
    QUdpSocket m_appService;
    QUdpSocket m_serialService;
    int m_baudRate;
};

#endif // XBEEPROPELLERCONNECTION_H
