#ifndef WIFIPROPCONNECTION_H
#define WIFIPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "sock.h"

#define WIFI_INITIAL_BAUD_RATE      115200
#define WIFI_FINAL_BAUD_RATE        921600
#define WIFI_TERMINAL_BAUD_RATE     115200

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
    bool isOpen();
    int close();
    int connect();
    int disconnect();
    int generateResetSignal();
    int identify(int *pVersion);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    int terminal(bool checkForExit, bool pstMode);
    static int findModules(bool check, WiFiInfoList &list);
private:
    int sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult);
    static void dumpHdr(const uint8_t *buf, int size);
    static void dumpResponse(const uint8_t *buf, int size);
    char *m_ipaddr;
    SOCKADDR_IN m_httpAddr;
    SOCKADDR_IN m_telnetAddr;
    SOCKET m_socket;
};

#endif // WIFIPROPELLERCONNECTION_H
