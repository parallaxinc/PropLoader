#ifndef PROPCONNECTION_H
#define PROPCONNECTION_H

#include <stdint.h>
#include <stdint.h>

typedef enum {
    ltShutdown = 0,
    ltDownloadAndRun = 1 << 0,
    ltDownloadAndProgram = 1 << 1,
    ltDownloadAndProgramAndRun = ltDownloadAndRun | ltDownloadAndProgram
} LoadType;

class PropConnection
{
public:
    PropConnection();
    ~PropConnection();
    virtual bool isOpen() = 0;
    virtual int close() = 0;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int generateResetSignal() = 0;
    virtual int identify(int *pVersion) = 0;
    virtual int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun) = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual int receiveDataTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int receiveDataExactTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int setBaudRate(int baudRate) = 0;
    virtual int maxDataSize() = 0;
    virtual int terminal(bool checkForExit, bool pstMode) = 0;
    int initialBaudRate() { return m_initialBaudRate; }
    void setInitialBaudRate(int baudRate) { m_initialBaudRate = baudRate; }
    int finalBaudRate() { return m_finalBaudRate; }
    void setFinalBaudRate(int baudRate) { m_finalBaudRate = baudRate; }
    int terminalBaudRate() { return m_terminalBaudRate; }
    void setTerminalBaudRate(int baudRate) { m_terminalBaudRate = baudRate; }
protected:
    int m_baudRate;
    int m_initialBaudRate;
    int m_finalBaudRate;
    int m_terminalBaudRate;
};

#endif // PROPCONNECTION_H
