#include <stdio.h>
#include "serialpropconnection.h"
#include "messages.h"

#define CALIBRATE_DELAY         10

SerialPropConnection::SerialPropConnection()
    : m_serialPort(NULL)
{
}

SerialPropConnection::~SerialPropConnection()
{
    close();
}

// this is in serialloader.cpp
// int SerialPropConnection::loadImage(const uint8_t *image, int imageSize, LoadType loadType);

struct FindState {
    SerialInfoList *list;
    int count;
};

int SerialPropConnection::addPort(const char *port, void *data)
{
    FindState *state = (FindState *)data;
    
    SerialInfo info(port);
    state->list->push_back(info);
    
    return state->count < 0 || --state->count > 0;
}

int SerialPropConnection::findPorts(bool check, SerialInfoList &list, int count)
{
    FindState state;
    state.list = &list;
    state.count = count;
    SerialFind(addPort, &state);
    return 0;
}

int SerialPropConnection::open(const char *port, int baudRate)
{
    if (isOpen())
        return -1;

    if (OpenSerial(port, baudRate, &m_serialPort) != 0)
        return -1;
        
    setPortName(port);
    m_baudRate = baudRate;

    return 0;
}

bool SerialPropConnection::isOpen()
{
    return m_serialPort ? true : false;
}

int SerialPropConnection::close()
{
    if (!isOpen())
        return -1;

    CloseSerial(m_serialPort);
    m_serialPort = NULL;

    return 0;
}

int SerialPropConnection::connect()
{
    return isOpen() ? 0 : -1;
}

int SerialPropConnection::disconnect()
{
    return 0;
}

int SerialPropConnection::setResetMethod(const char *method)
{
    if (SerialUseResetMethod(m_serialPort, method) != 0)
        return -1;
    return 0;
}

int SerialPropConnection::generateResetSignal()
{
    if (!isOpen())
        return -1;
    SerialGenerateResetSignal(m_serialPort);
    return 0;
}

int SerialPropConnection::sendData(const uint8_t *buf, int len)
{
    if (!isOpen())
        return -1;
    return SendSerialData(m_serialPort, buf, len);
}

int SerialPropConnection::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSerialDataTimeout(m_serialPort, buf, len, timeout);
}

int SerialPropConnection::receiveDataExactTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSerialDataExactTimeout(m_serialPort, buf, len, timeout);
}

int SerialPropConnection::receiveChecksumAck(int byteCount, int delay)
{
    static uint8_t calibrate[1] = { 0xF9 };
    int msSendTime = (byteCount * 10 * 1000) / m_baudRate;
    int retries = (msSendTime / CALIBRATE_DELAY) | (delay / CALIBRATE_DELAY);
    uint8_t buf[1];
    int cnt;

    do {
        sendData(calibrate, sizeof(calibrate));
        cnt = receiveDataExactTimeout(buf, 1, CALIBRATE_DELAY);
        if (cnt == 1)
            return buf[0] == 0xFE ? 0 : -1;
    } while (--retries >= 0);

    return -1;
}

int SerialPropConnection::setBaudRate(int baudRate)
{
     if (baudRate != m_baudRate) {
        FlushSerialData(m_serialPort);
        if (SetSerialBaud(m_serialPort, baudRate) != 0)
            return -1;
        m_baudRate = baudRate;
    }
    return 0;
}

int SerialPropConnection::terminal(bool checkForExit, bool pstMode)
{
    SerialTerminal(m_serialPort, checkForExit, pstMode);
    return 0;
}
