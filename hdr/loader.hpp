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
    int loadImage(const uint8_t *image, int imageSize);
protected:
    virtual int connect() = 0;
    virtual void disconnect() = 0;
    virtual int generateResetSignal() = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual int receiveData(uint8_t *buf, int len) = 0;
    virtual int receiveDataExact(uint8_t *buf, int len, int timeout) = 0;
    int m_baudrate;
private:
    uint8_t *generateInitialLoaderPacket(int *pLength, int *pPacketID, int *pChecksum);
    int transmitPacket(int id, void *payload, int payloadSize);
};

#endif
