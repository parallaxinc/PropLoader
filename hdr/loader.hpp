#ifndef __LOADER_HPP__
#define __LOADER_HPP__

#include <stdint.h>

#define DEFAULT_BAUDRATE    115200

class Loader {
public:
    Loader() : m_baudrate(DEFAULT_BAUDRATE) {}
    ~Loader() {}
    virtual int init(int baudrate = DEFAULT_BAUDRATE);
    int loadFile(const char *file);
    int loadFile2(const char *file);
    int loadImage(const uint8_t *image, int imageSize);
    int loadImage2(const uint8_t *image, int imageSize);
protected:
    virtual int connect() = 0;
    virtual void disconnect() = 0;
    virtual int setBaudRate(int baudrate) = 0;
    virtual int generateResetSignal() = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual int receiveData(uint8_t *buf, int len) = 0;
    virtual int receiveDataExact(uint8_t *buf, int len, int timeout) = 0;
    virtual int maxDataSize() = 0;
    int m_baudrate;
private:
    uint8_t *readEntireFile(const char *file, int *pLength);
    uint8_t *generateInitialLoaderPacket(int packetID, int *pLength);
    int transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult);
};

#endif
