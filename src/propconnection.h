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
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int generateResetSignal() = 0;
    virtual int identify(int *pVersion) = 0;
    virtual int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun) = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual int receiveDataTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int receiveDataExactTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int initialBaudRate() = 0;
    virtual int setBaudRate(int baudRate) = 0;
    virtual int maxDataSize() = 0;
    virtual void terminal(bool checkForExit, bool pstMode) = 0;
};

#endif // PROPCONNECTION_H
