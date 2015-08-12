#ifndef __XBEE_LOADER_H__
#define __XBEE_LOADER_H__

#include <string>
#include <list>

#include "loader.hpp"
#include "xbee.hpp"

#define XBEE_MAX_DATA_SIZE   1392        /* The default maximum size for a single UDP packet. */

class XbeeInfo {
friend class XbeeLoader;
public:
    XbeeInfo()
    : m_hostAddr(0),
      m_xbeeAddr(0),
      m_macAddrHigh(0),
      m_macAddrLow(0),
      m_xbeePort(0),
      m_firmwareVersion(0),
      m_cfgChecksum(0) {}
    XbeeInfo(XbeeAddr addr)
    : m_hostAddr(addr.hostAddr()),
      m_xbeeAddr(addr.xbeeAddr()),
      m_macAddrHigh(0),
      m_macAddrLow(0),
      m_xbeePort(0),
      m_firmwareVersion(0),
      m_cfgChecksum(0) {}
    uint32_t hostAddr()         { return m_hostAddr; }
    uint32_t xbeeAddr()         { return m_xbeeAddr; }
    uint32_t macAddrHigh()      { return m_macAddrHigh; }
    uint32_t macAddrLow()       { return m_macAddrLow; }
    uint16_t xbeePort()         { return m_xbeePort; }
    uint32_t firmwareVersion()  { return m_firmwareVersion; }
    uint32_t cfgChecksum()      { return m_cfgChecksum; }
    const std::string &nodeID() { return m_nodeID; }
    const std::string &name()   { return m_name; }
private:
    uint32_t m_hostAddr;
    uint32_t m_xbeeAddr;
    uint32_t m_macAddrHigh;
    uint32_t m_macAddrLow;
    uint16_t m_xbeePort;
    uint32_t m_firmwareVersion;
    uint32_t m_cfgChecksum;
    std::string m_nodeID;
    std::string m_name;
};

typedef std::list<XbeeInfo> XbeeInfoList;

#define SERIAL_MAX_DATA_SIZE   1020

class XbeeLoader : public Loader {
public:
    XbeeLoader();
    ~XbeeLoader();
    int discover(bool check, XbeeInfoList &list, int timeout = DEF_DISCOVER_TIMEOUT);
    int connect(XbeeAddr &addr, int baudrate = DEFAULT_BAUDRATE);
    void disconnect();
    void terminal(bool checkForExit, bool pstMode);
protected:
    int setBaudRate(int baudrate);
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveData(uint8_t *buf, int len);
    int receiveDataExact(uint8_t *buf, int len, int timeout);
    int maxDataSize() { return XBEE_MAX_DATA_SIZE; }
private:
    int enforceXbeeConfiguration(Xbee &xbee);
    XbeeAddr m_addr;
    Xbee m_xbee;
};

#endif
