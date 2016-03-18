#ifndef SERIALPROPCONNECTION_H
#define SERIALPROPCONNECTION_H

#include <string>
#include <list>
#include "propconnection.h"
#include "serial.h"

#define SERIAL_INITIAL_BAUD_RATE    115200

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
    int open(const char *port, int baudRate = SERIAL_INITIAL_BAUD_RATE);
    bool isOpen();
    int connect();
    int disconnect();
    int generateResetSignal();
    int identify(int *pVersion);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int initialBaudRate() { return SERIAL_INITIAL_BAUD_RATE; }
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    void terminal(bool checkForExit, bool pstMode);
    static int findPorts(const char *prefix, bool check, SerialInfoList &list);
private:
    int receiveChecksumAck(int byteCount, int delay);
    static int addPort(const char *port, void *data);
    SERIAL *m_serialPort;
    int m_baudRate;
};

#endif // SERIALPROPELLERCONNECTION_H
