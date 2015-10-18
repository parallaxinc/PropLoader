#ifndef PROPELLERLOADER_H
#define PROPELLERLOADER_H

#include <stdint.h>
#include <QObject>
#include <QByteArray>
#include "propellerimage.h"
#include "propellerconnection.h"
#include "loadelf.h"

enum LoadType {
    ltNone = 0,
    ltDownloadAndRun = (1 << 0),
    ltDownloadAndProgram = (1 << 1),
    ltDownloadAndProgramAndRun = ltDownloadAndRun | ltDownloadAndProgram
};

class PropellerLoader : public QObject
{
    Q_OBJECT

public:
    PropellerLoader(PropellerConnection &connection);
    ~PropellerLoader();
    int load(PropellerImage &image, LoadType loadType);

private:
    static void generateIdentifyPacket(QByteArray &packet);
    static int generateLoaderPacket(QByteArray &packet, const uint8_t *image, int imageSize, LoadType loadType);
    static void encodeBytes(QByteArray &packet, const uint8_t *inBytes, int inCount);

    PropellerConnection &m_connection;
};

#endif // PROPELLERLOADER_H
