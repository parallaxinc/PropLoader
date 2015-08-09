/* Propeller WiFi loader

  Based on Jeff Martin's Pascal loader and Mike Westerfield's iOS loader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include "serial-loader.hpp"

SerialLoader::SerialLoader() : m_port(NULL), m_serial(NULL)
{
}

SerialLoader::~SerialLoader()
{
    disconnect();
    if (m_port)
        free(m_port);
}

int SerialLoader::init(const char *port, int baudrate)
{
    setBaudrate(baudrate);
    if (m_port) {
        free(m_port);
        m_port = NULL;
    }
    if (!(m_port = (char *)malloc(strlen(port) + 1)))
        return -1;
    strcpy(m_port, port);
    return 0;
}

int SerialLoader::connect()
{
    int sts;
    if (!m_port || m_serial)
        return -1;
    if ((sts = OpenSerial(m_port, m_baudrate, &m_serial)) != 0)
        return sts;
    return 0;
}

void SerialLoader::disconnect()
{
    if (m_serial) {
        CloseSerial(m_serial);
        m_serial = NULL;
    }
}

int SerialLoader::setBaudRate(int baudrate)
{
    return m_serial ? SetSerialBaud(m_serial, baudrate) : -1;
}

int SerialLoader::generateResetSignal()
{
    return m_serial ? SerialGenerateResetSignal(m_serial) : -1;
}

int SerialLoader::sendData(const uint8_t *buf, int len)
{
    return m_serial ? SendSerialData(m_serial, buf, len) : -1;
}

int SerialLoader::receiveData(uint8_t *buf, int len)
{
    return m_serial ? ReceiveSerialData(m_serial, buf, len) : -1;
}

int SerialLoader::receiveDataExact(uint8_t *buf, int len, int timeout)
{
    return m_serial ? ReceiveSerialDataExact(m_serial, buf, len, timeout) : -1;
}

struct FindState {
    SerialLoader *ldr;
    bool check;
    SerialInfoList *list;
};

int SerialLoader::addPort(const char *port, void *data)
{
    FindState *state = (FindState *)data;
    
    if (state->check) {
        int version;
        if (state->ldr->init(port) != 0)
            return 1;
        if (state->ldr->identify(&version) != 0 || version != 1)
            return 1;
    }
    
    SerialInfo info(port);
    state->list->push_back(info);
    
    return 1;
}

int SerialLoader::findPorts(const char *prefix, bool check, SerialInfoList &list)
{
    FindState state;
    state.ldr = this;
    state.check = check;
    state.list = &list;
    SerialFind(prefix, addPort, &state);
    return 0;
}
