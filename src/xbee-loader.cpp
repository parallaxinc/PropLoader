/* Propeller WiFi loader

  Based on Jeff Martin's Pascal loader and Mike Westerfield's iOS loader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include "xbee-loader.hpp"

XbeeLoader::XbeeLoader()
{
}

XbeeLoader::~XbeeLoader()
{
}

int XbeeLoader::init(XBEE_ADDR *addr, int baudrate)
{
    setBaudrate(baudrate);
    m_addr = *addr;
    return 0;
}

int XbeeLoader::connect()
{
    return m_xbee.connect(&m_addr);
}

void XbeeLoader::disconnect()
{
    m_xbee.disconnect();
}

int XbeeLoader::setBaudRate(int baudrate)
{
    return m_xbee.setItem(xbSerialBaud, baudrate);
}

int XbeeLoader::generateResetSignal()
{
    if (enforceXbeeConfiguration(m_xbee) != 0)
        return -1;
    return m_xbee.setItem(xbOutputState, 0x0010);
}

int XbeeLoader::sendData(const uint8_t *buf, int len)
{
    int packetSize, cnt;
    uint8_t *packet;
    appHeader *hdr;
    
    /* allocate a packet big enough to include the app header */
    packetSize = sizeof(appHeader) + len;
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return -1;
    
    /* initialize the app header */
    hdr = (appHeader *)packet;
    hdr->number1 = 0x0000;
    hdr->number2 = hdr->number1 ^ 0x4242;
    hdr->packetID = 0;
    hdr->encryptionPad = 0;
    hdr->commandID = 0x00;
    hdr->commandOptions = 0x00;
    
    /* copy the data into the packet */
    memcpy(packet + sizeof(appHeader), buf, len);
    
    /* send the data to the xbee */
    cnt = m_xbee.sendAppData(packet, packetSize);
    
    /* free the the packet */
    free(packet);
    
    /* return the number of bytes sent */
    return cnt;
}

int XbeeLoader::receiveData(uint8_t *buf, int len)
{
    return m_xbee.receiveSerialData(buf, len);
}

int XbeeLoader::receiveDataExact(uint8_t *buf, int len, int timeout)
{
    return m_xbee.receiveSerialData(buf, len);
}

static int validate(Xbee &xbee, xbCommand cmd, int value, bool readOnly)
{
    int currentValue;
    if (xbee.getItem(cmd, &currentValue) != 0)
        return -1;
    if (currentValue != value && !readOnly) {
        if (xbee.setItem(cmd, value) != 0)
            return -1;
    }
    return 0;
}

int XbeeLoader::enforceXbeeConfiguration(Xbee &xbee)
{
    if (validate(xbee, xbSerialIP, serialUDP, true) != 0                // Ensure XBee's Serial Service uses UDP packets [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbee, xbIPDestination, m_addr.host, false) != 0        // Ensure Serial-to-IP destination is us (our IP)
    ||  validate(xbee, xbIPPort, DEF_SERIAL_SERVICE_PORT, false) != 0   // Ensure Serial-to-IP port is proper (default, in this case)
    ||  validate(xbee, xbOutputMask, 0x7FFF, false) != 0                // Ensure output mask is proper (default, in this case)
    ||  validate(xbee, xbRTSFlow, pinEnabled, false) != 0               // Ensure RTS flow pin is enabled (input)
    ||  validate(xbee, xbIO4Mode, pinOutLow, false) != 0                // Ensure serial hold pin is set to output low
    ||  validate(xbee, xbIO2Mode, pinOutHigh, false) != 0               // Ensure reset pin is set to output high
    ||  validate(xbee, xbIO4Timer, 2, false) != 0                       // Ensure serial hold pin's timer is set to 200 ms
    ||  validate(xbee, xbIO2Timer, 1, false) != 0                       // Ensure reset pin's timer is set to 100 ms
    ||  validate(xbee, xbSerialMode, transparentMode, true) != 0        // Ensure Serial Mode is transparent [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbee, xbSerialBaud, m_baudrate, false) != 0            // Ensure baud rate is set to initial speed
    ||  validate(xbee, xbSerialParity, parityNone, false) != 0          // Ensure parity is none
    ||  validate(xbee, xbSerialStopBits, stopBits1, false) != 0         // Ensure stop bits is 1
    ||  validate(xbee, xbPacketingTimeout, 3, false) != 0)              // Ensure packetization timout is 3 character times
        return -1;
    return 0;
}
