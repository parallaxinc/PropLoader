#ifndef PROPELLERLOADER_H
#define PROPELLERLOADER_H

#include <stdint.h>
#include <QObject>
#include <QByteArray>
#include "propellerconnection.h"

class PropellerLoader : public QObject
{
    Q_OBJECT

public:
    PropellerLoader(PropellerConnection &connection);
    ~PropellerLoader();
    int load(const char *fileName);
    int load(uint8_t *image, int size);

private:
    static void generateIdentifyPacket(QByteArray &packet);
    static void generateLoaderPacket(QByteArray &packet, const uint8_t *image, int imageSize);
    static void encodeBytes(QByteArray &packet, const uint8_t *inBytes, int inCount);

    PropellerConnection &m_connection;
    int m_baudRate;
};

#endif // PROPELLERLOADER_H
