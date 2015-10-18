#include <QThread>
#include "serialpropellerconnection.h"

// number of milliseconds between attempts to read the checksum ack
#define CALIBRATE_PAUSE 10

SerialPropellerConnection::SerialPropellerConnection()
{
}

SerialPropellerConnection::SerialPropellerConnection(const char *port, int baudRate)
{
    open(port, baudRate);
}

QStringList SerialPropellerConnection::availablePorts(const char *prefix)
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    QStringList portNames;

    for (int i = 0; i < ports.size(); ++i) {
        QString name = ports[i].systemLocation();
        if (name.startsWith(prefix))
            portNames.append(name);
    }

    return portNames;
}

int SerialPropellerConnection::open(const char *port, int baudRate)
{
    if (isOpen())
        close();

    m_baudRate = baudRate;
    m_serialPort.setPortName(port);
    m_serialPort.setBaudRate(m_baudRate);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setParity(QSerialPort::NoParity);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    m_serialPort.open(QIODevice::ReadWrite);

    if (m_serialPort.isOpen()) {
        m_serialPort.setBreakEnabled(false);
        m_serialPort.setDataTerminalReady(false);
    }

    return m_serialPort.isOpen() ? 0 : -1;
}

bool SerialPropellerConnection::isOpen()
{
    return m_serialPort.isOpen();
}

int SerialPropellerConnection::close()
{
    if (isOpen())
        m_serialPort.close();
    return 0;
}

SerialPropellerConnection::~SerialPropellerConnection()
{
    close();
}

int SerialPropellerConnection::generateResetSignal()
{
    if (!isOpen())
        return -1;
    m_serialPort.setDataTerminalReady(true);
    QThread::msleep(10);
    m_serialPort.setDataTerminalReady(false);
    QThread::msleep(100);
    m_serialPort.clear();
    return 0;
}

int SerialPropellerConnection::sendData(const uint8_t *buf, int len)
{
    if (!isOpen())
        return -1;
    return m_serialPort.write((char *)buf, len);
}

void SerialPropellerConnection::pauseForVerification(int byteCount)
{
    Q_UNUSED(byteCount);
    //int msDelay = (byteCount * 10 * 1000) / m_baudRate;
    //if (isOpen())
    //    m_serialPort.waitForBytesWritten(msDelay);
    //QThread::msleep(100);
}

int SerialPropellerConnection::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    if (!m_serialPort.waitForReadyRead(timeout))
        return -1;
    return (int)m_serialPort.read((char *)buf, len);
}

int SerialPropellerConnection::receiveDataExactTimeout(uint8_t *buf, int len, int timeout)
{
    int remaining = len;

    if (!isOpen())
        return -1;

    /* return only when the buffer contains the exact amount of data requested */
    while (remaining > 0) {
        int cnt;

        /* wait for data */
        if (!m_serialPort.waitForReadyRead(timeout))
            return -1;

        /* read the next bit of data */
        if ((cnt = (int)m_serialPort.read((char *)buf, remaining)) < 0)
            return -1;

        /* update the buffer pointer */
        remaining -= cnt;
        buf += cnt;
    }

    /* return the full size of the buffer */
    return len;
}

int SerialPropellerConnection::receiveChecksumAck(int byteCount, int delay)
{
    static uint8_t calibrate[1] = { 0xF9 };
    int msSendTime = (byteCount * 10 * 1000) / m_baudRate;
    int retries = (msSendTime / CALIBRATE_PAUSE) | (delay / CALIBRATE_PAUSE);
    uint8_t buf[1];
    int cnt;

    do {
        sendData(calibrate, sizeof(calibrate));
        cnt = receiveDataExactTimeout(buf, 1, CALIBRATE_PAUSE);
        if (cnt == 1)
            return buf[0] == 0xFE ? 0 : -1;
    } while (--retries >= 0);

    return -1;
}

int SerialPropellerConnection::setBaudRate(int baudRate)
{
    return m_serialPort.setBaudRate(baudRate) ? 0 : -1;
}


