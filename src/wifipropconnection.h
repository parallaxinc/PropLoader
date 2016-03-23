#ifndef WIFIPROPCONNECTION_H
#define WIFIPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "sock.h"

#define WIFI_LOADER_BAUD_RATE       115200
#define WIFI_FAST_LOADER_BAUD_RATE  921600
#define WIFI_PROGRAM_BAUD_RATE      115200

class WiFiInfo {
public:
    WiFiInfo() {}
    WiFiInfo(std::string name, std::string address) : m_name(name), m_address(address) {}
    const char *name() { return m_name.c_str(); }
    const char *address() { return m_address.c_str(); }
private:
    std::string m_name;
    std::string m_address;
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
    int setName(const char *name);
    int generateResetSignal();
    int identify(int *pVersion);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    int terminal(bool checkForExit, bool pstMode);
    static int findModules(bool check, WiFiInfoList &list, int count = -1);
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
