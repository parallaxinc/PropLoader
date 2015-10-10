#ifndef PROPELLERCONNECTION_H
#define PROPELLERCONNECTION_H

#include <stdint.h>
#include <QObject>

class PropellerConnection : public QObject
{
    Q_OBJECT

public:
    PropellerConnection();
    ~PropellerConnection();
    virtual bool isOpen() = 0;
    virtual int close() = 0;
    virtual int generateResetSignal() = 0;
    virtual int sendData(const uint8_t *buf, int len) = 0;
    virtual void pauseForVerification(int byteCount) = 0;
    virtual int receiveDataTimeout(uint8_t *buf, int len, int timeout) = 0;
    virtual int receiveDataExactTimeout(uint8_t *buf, int len, int timeout) = 0;
};

#endif // PROPELLERCONNECTION_H
