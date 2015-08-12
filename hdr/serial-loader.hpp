#ifndef __SERIAL_LOADER_H__
#define __SERIAL_LOADER_H__

#include <string>
#include <list>

#include "loader.hpp"
#include "serial.h"

#define SERIAL_MAX_DATA_SIZE   1020

class SerialInfo {
public:
    SerialInfo() {}
    SerialInfo(std::string port) : m_port(port) {}
    const char *port() { return m_port.c_str(); }
private:
    std::string m_port;
};

typedef std::list<SerialInfo> SerialInfoList;

#define SERIAL_MAX_DATA_SIZE   1020

class SerialLoader : public Loader {
public:
    SerialLoader();
    ~SerialLoader();
    int findPorts(const char *prefix, bool check, SerialInfoList &list);
    int connect(const char *port, int baudrate = DEFAULT_BAUDRATE);
    void disconnect();
    void terminal(bool checkForExit, bool pstMode);
protected:
    int setBaudRate(int baudrate);
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveData(uint8_t *buf, int len);
    int receiveDataExact(uint8_t *buf, int len, int timeout);
    int maxDataSize() { return SERIAL_MAX_DATA_SIZE; }
    static int addPort(const char *port, void *data);
private:
    SERIAL *m_serial;
};

#endif
