#ifndef __XBEE_HPP__
#define __XBEE_HPP__

#include <stdint.h>
#include "sock.h"

#define DEF_APP_SERVICE_PORT    0xBEE
#define DEF_SERIAL_SERVICE_PORT 0x2616

typedef struct {
    uint32_t host;
    uint32_t xbee;
} XBEE_ADDR;

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

class Xbee {
public:
    Xbee();
    ~Xbee();
    static int discover(XBEE_ADDR *addrs, int max, int timeout);
    int connect(XBEE_ADDR *addr);
    void disconnect();
    int getItem(xbCommand cmd, int *pValue);
    int setItem(xbCommand cmd, int value);
    int sendAppData(void *buf, int len);
    int receiveAppData(void *buf, int len);
    int receiveSerialData(void *buf, int len);
private:
    static int discover1(IFADDR *ifaddr, XBEE_ADDR *addrs, int max, int timeout);
    SOCKET m_appService;
    SOCKET m_serialService;
};

#endif
