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
  : m_addr(0, 0)
{
}

XbeeLoader::~XbeeLoader()
{
}

int XbeeLoader::connect(XbeeAddr &addr, int baudrate)
{
    m_addr = addr;
    setBaudrate(baudrate);
    return m_xbee.connect(m_addr.xbeeAddr());
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

void XbeeLoader::pauseForVerification(int byteCount)
{
    /* Reset period 200 ms + first packet’s serial transfer time + 20 ms */
    msleep(200 + (byteCount * 10 * 1000) / m_baudrate + 20);
}

void XbeeLoader::pauseForChecksum(int byteCount)
{
    /* this delay helps apply the majority of the next step’s receive timeout to a
       valid time window in the communication sequence */
    msleep((byteCount * 10 * 1000) / m_baudrate);
}

int XbeeLoader::receiveData(uint8_t *buf, int len)
{
    return m_xbee.receiveSerialData(buf, len);
}

int XbeeLoader::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    return m_xbee.receiveSerialDataTimeout(buf, len, timeout);
}

int XbeeLoader::receiveDataExact(uint8_t *buf, int len, int timeout)
{
    return m_xbee.receiveSerialDataTimeout(buf, len, timeout);
}

void XbeeLoader::terminal(bool checkForExit, bool pstMode)
{
    m_xbee.terminal(checkForExit, pstMode);
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
printf("host ip %08x\n", ntohl(m_addr.hostAddr()));
    if (validate(xbee, xbSerialIP, serialUDP, true) != 0                        // Ensure XBee's Serial Service uses UDP packets [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbee, xbIPDestination, ntohl(m_addr.hostAddr()), false) != 0   // Ensure Serial-to-IP destination is us (our IP)
    ||  validate(xbee, xbIPPort, DEF_SERIAL_SERVICE_PORT, false) != 0           // Ensure Serial-to-IP port is proper (default, in this case)
    ||  validate(xbee, xbOutputMask, 0x7FFF, false) != 0                        // Ensure output mask is proper (default, in this case)
    ||  validate(xbee, xbRTSFlow, pinEnabled, false) != 0                       // Ensure RTS flow pin is enabled (input)
    ||  validate(xbee, xbIO4Mode, pinOutLow, false) != 0                        // Ensure serial hold pin is set to output low
    ||  validate(xbee, xbIO2Mode, pinOutHigh, false) != 0                       // Ensure reset pin is set to output high
    ||  validate(xbee, xbIO4Timer, 2, false) != 0                               // Ensure serial hold pin's timer is set to 200 ms
    ||  validate(xbee, xbIO2Timer, 1, false) != 0                               // Ensure reset pin's timer is set to 100 ms
    ||  validate(xbee, xbSerialMode, transparentMode, true) != 0                // Ensure Serial Mode is transparent [WRITE DISABLED DUE TO FIRMWARE BUG]
    ||  validate(xbee, xbSerialBaud, m_baudrate, false) != 0                    // Ensure baud rate is set to initial speed
    ||  validate(xbee, xbSerialParity, parityNone, false) != 0                  // Ensure parity is none
    ||  validate(xbee, xbSerialStopBits, stopBits1, false) != 0                 // Ensure stop bits is 1
    ||  validate(xbee, xbPacketingTimeout, 3, false) != 0)                      // Ensure packetization timout is 3 character times
        return -1;
    return 0;
}

int XbeeLoader::discover(bool check, XbeeInfoList &list, int timeout)
{
    XbeeAddrList addrs;
    if (m_xbee.discover(addrs, timeout) == 0) {
        XbeeAddrList::iterator i = addrs.begin();
        while (i != addrs.end()) {
            std::string sValue;
            int value;
            XbeeInfo info(*i);
            m_xbee.connect(i->xbeeAddr());
            if (m_xbee.getItem(xbMacHigh, &value) == 0)
                info.m_macAddrHigh = value;
            if (m_xbee.getItem(xbMacLow, &value) == 0)
                info.m_macAddrLow = value;
            if (m_xbee.getItem(xbIPPort, &value) == 0)
                info.m_xbeePort = value;
            if (m_xbee.getItem(xbFirmwareVer, &value) == 0)
                info.m_firmwareVersion = value;
            if (m_xbee.getItem(xbChecksum, &value) == 0)
                info.m_cfgChecksum = value;
            if (m_xbee.getItem(xbNodeID, sValue) == 0)
                info.m_nodeID = sValue;
            m_xbee.disconnect();
            if (check) {
                XbeeAddr addr(info.hostAddr(), info.xbeeAddr());
                int version;
                connect(addr);
                if (identify(&version) == 0 && version == 1)
                    list.push_back(info);
                disconnect();
            }
            else
                list.push_back(info);
            ++i;
        }
    }
    return 0;
}
