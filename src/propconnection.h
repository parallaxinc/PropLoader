#ifndef PROPCONNECTION_H
#define PROPCONNECTION_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    ltShutdown = 0,
    ltDownloadAndRun = 1 << 0,
    ltDownloadAndProgram = 1 << 1,
    ltDownloadAndProgramAndRun = ltDownloadAndRun | ltDownloadAndProgram
} LoadType;

class PropConnection
{
public:
    PropConnection() : m_portName(NULL) {}
    ~PropConnection() { if (m_portName) free(m_portName); }
    virtual bool isOpen() = 0;
    virtual int close() = 0;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int generateResetSignal() = 0;
    virtual int identify(int *pVersion) = 0;
    virtual int loadImage(const uint8_t *image, int imageSize, uint8_t *response, int responseSize) = 0;
    virtual int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun) = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual int receiveDataTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int receiveDataExactTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int setBaudRate(int baudRate) = 0;
    virtual int maxDataSize() = 0;
    virtual int terminal(bool checkForExit, bool pstMode) = 0;
    const char *portName() { return m_portName ? m_portName : "<none>"; }
    void setPortName(const char *portName) {
        if (m_portName)
            free(m_portName);
        if ((m_portName = (char *)malloc(strlen(portName) + 1)) != NULL)
            strcpy(m_portName, portName);
    }
    int loaderBaudRate() { return m_loaderBaudRate; }
    void setLoaderBaudRate(int baudRate) { m_loaderBaudRate = baudRate; }
    int fastLoaderBaudRate() { return m_fastLoaderBaudRate; }
    void setFastLoaderBaudRate(int baudRate) { m_fastLoaderBaudRate = baudRate; }
    int programBaudRate() { return m_programBaudRate; }
    void setProgramBaudRate(int baudRate) { m_programBaudRate = baudRate; }
protected:
    char *m_portName;
    int m_baudRate;
    int m_loaderBaudRate;
    int m_fastLoaderBaudRate;
    int m_programBaudRate;
};

#endif // PROPCONNECTION_H
