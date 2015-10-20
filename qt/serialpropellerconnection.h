#ifndef SERIALPROPELLERCONNECTION_H
#define SERIALPROPELLERCONNECTION_H

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include "propellerconnection.h"

class SerialPropellerConnection : public PropellerConnection
{
    Q_OBJECT

public:
    SerialPropellerConnection();
    SerialPropellerConnection(const char *port, int baudRate = 115200);
    ~SerialPropellerConnection();
    int open(const char *port, int baudRate = 115200);
    bool isOpen();
    int close();
    int generateResetSignal();
    int sendData(const uint8_t *buf, int len);
    int receiveDataTimeout(uint8_t *buf, int len, int timeout);
    int receiveDataExactTimeout(uint8_t *buf, int len, int timeout);
    int receiveChecksumAck(int byteCount, int delay);
    int setBaudRate(int baudRate);
    int maxDataSize() { return 1024; }
    static QStringList availablePorts(const char *prefix);
private:
    QSerialPort m_serialPort;
    int m_baudRate;
};

#endif // SERIALPROPELLERCONNECTION_H
