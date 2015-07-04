#ifndef __WIFI_LOADER_H__
#define __WIFI_LOADER_H__

#include "loader.hpp"
#include "xbee.hpp"

class XbeeLoader : public Loader {
public:
    XbeeLoader();
    ~XbeeLoader();
    int init(const char *ipaddr, int baudrate = DEFAULT_BAUDRATE);
protected:
    int connect();
    void disconnect();
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveData(uint8_t *buf, int len);
    int receiveDataExact(uint8_t *buf, int len, int timeout);
private:
    int enforceXbeeConfiguration(Xbee &xbee);
    char *m_ipaddr;
    Xbee m_xbee;
};

#endif
