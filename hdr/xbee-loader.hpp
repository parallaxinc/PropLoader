#ifndef __XBEE_LOADER_H__
#define __XBEE_LOADER_H__

#include "loader.hpp"
#include "xbee.hpp"

#define XBEE_MAX_DATA_SIZE   1392        /* The default maximum size for a single UDP packet. */

class XbeeLoader : public Loader {
public:
    XbeeLoader();
    ~XbeeLoader();
    int init(const char *ipaddr, int baudrate = DEFAULT_BAUDRATE);
protected:
    int connect();
    void disconnect();
    int setBaudRate(int baudrate);
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveData(uint8_t *buf, int len);
    int receiveDataExact(uint8_t *buf, int len, int timeout);
    int maxDataSize() { return XBEE_MAX_DATA_SIZE; }
private:
    int enforceXbeeConfiguration(Xbee &xbee);
    char *m_ipaddr;
    Xbee m_xbee;
};

#endif
