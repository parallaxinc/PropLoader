#ifndef SERIALPROPCONNECTION_H
#define SERIALPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "serial.h"

class SerialInfo {
public:
    SerialInfo() {}
    SerialInfo(std::string port) : m_port(port) {}
    const char *port() { return m_port.c_str(); }
private:
    std::string m_port;
};

typedef std::list<SerialInfo> SerialInfoList;

class SerialPropConnection : public PropConnection
{
public:
    SerialPropConnection();
    ~SerialPropConnection();
    int open(const char *port, int baudRate);
    bool isOpen();
    int close();
    int connect();
    int disconnect();
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
    static int findPorts(bool check, SerialInfoList &list, int count = -1);
private:
    int receiveChecksumAck(int byteCount, int delay);
    static int addPort(const char *port, void *data);
    SERIAL *m_serialPort;
};

#endif // SERIALPROPELLERCONNECTION_H
