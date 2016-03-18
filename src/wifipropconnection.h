#ifndef WIFIPROPCONNECTION_H
#define WIFIPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "sock.h"

#define WIFI_INITIAL_BAUD_RATE  115200

class WiFiInfo {
public:
    WiFiInfo() {}
    WiFiInfo(std::string port) : m_port(port) {}
    const char *port() { return m_port.c_str(); }
private:
    std::string m_port;
};

typedef std::list<WiFiInfo> WiFiInfoList;

class WiFiPropConnection : public PropConnection
{
public:
    WiFiPropConnection();
    ~WiFiPropConnection();
    int setAddress(const char *ipaddr);
    int setPort(short port);
    bool isOpen();
    int connect();
    int disconnect();
    int generateResetSignal();
    int identify(int *pVersion);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int initialBaudRate() { return WIFI_INITIAL_BAUD_RATE; }
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    void terminal(bool checkForExit, bool pstMode);
    int findModules(const char *prefix, bool check, WiFiInfoList &list);
private:
    int sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult);
    static void dumpHdr(const uint8_t *buf, int size);
    static void dumpResponse(const uint8_t *buf, int size);
    static int addPort(const char *port, void *data);
    char *m_ipaddr;
    SOCKADDR_IN m_addr;
    bool m_readyToConnect;
    SOCKET m_socket;
    int m_baudRate;
};

#endif // WIFIPROPELLERCONNECTION_H
