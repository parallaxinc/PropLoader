#ifndef FASTPROPELLERLOADER_H
#define FASTPROPELLERLOADER_H

#include <stdint.h>
#include <QByteArray>

#include "propellerimage.h"
#include "propellerconnection.h"
#include "propellerloader.h"

class FastPropellerLoader
{
public:
    FastPropellerLoader(PropellerConnection &connection);
    ~FastPropellerLoader();
    int load(PropellerImage &image, LoadType loadType);

private:
    int transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout = 2000);
    int generateInitialLoaderImage(PropellerImage &image, int packetID, int baudRate);
    int maxDataSize() { return m_connection.maxDataSize(); }

    static void setHostInitializedValue(uint8_t *bytes, int offset, int value);
    static int32_t getLong(const uint8_t *buf);
    static void setLong(uint8_t *buf, uint32_t value);

    PropellerConnection &m_connection;
};

#endif // FASTPROPELLERLOADER_H
