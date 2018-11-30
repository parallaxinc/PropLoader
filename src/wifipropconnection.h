#ifndef WIFIPROPCONNECTION_H
#define WIFIPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "sock.h"

#define WIFI_REQUIRED_MAJOR_VERSION         "v1."
#define WIFI_REQUIRED_MAJOR_VERSION_LEGACY  "02-"

// timeout used when making an HTTP request or connecting a telnet session
#define CONNECT_TIMEOUT             3000
#define RESPONSE_TIMEOUT            3000
#define DISCOVER_REPLY_TIMEOUT      250
#define DISCOVER_ATTEMPTS           3

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
    int getVersion();
    int checkVersion();
    const char *version() { return m_version ? m_version : "(unknown)"; }
    bool isOpen();
    int close();
    int connect();
    int disconnect();
    int setName(const char *name);
    int setResetMethod(const char *method);
    int generateResetSignal();
    int identify(int *pVersion);
    int loadImage(const uint8_t *image, int imageSize, uint8_t *response, int responseSize);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun, int info = false);
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    int terminal(bool checkForExit, bool pstMode);
    static int findModules(bool show, WiFiInfoList &list, int count = -1);
private:
    int sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult);
    static uint8_t *getBody(uint8_t *msg, int msgSize, int *pBodySize);
    static void dumpHdr(const uint8_t *buf, int size);
    static void dumpResponse(const uint8_t *buf, int size);
    char *m_ipaddr;
    char *m_version;
    SOCKADDR_IN m_httpAddr;
    SOCKADDR_IN m_telnetAddr;
    SOCKET m_telnetSocket;
    int m_resetPin;
};

#endif // WIFIPROPELLERCONNECTION_H
