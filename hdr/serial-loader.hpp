#ifndef __SERIAL_LOADER_H__
#define __SERIAL_LOADER_H__

#include "loader.hpp"
#include "serial.h"

#define SERIAL_MAX_DATA_SIZE   1020

class SerialLoader : public Loader {
public:
    SerialLoader();
    ~SerialLoader();
    int init(const char *port, int baudrate = DEFAULT_BAUDRATE);
protected:
    int connect();
    void disconnect();
    int setBaudRate(int baudrate);
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveData(uint8_t *buf, int len);
    int receiveDataExact(uint8_t *buf, int len, int timeout);
    int maxDataSize() { return SERIAL_MAX_DATA_SIZE; }
private:
    char *m_port;
    SERIAL *m_serial;
};

#endif
