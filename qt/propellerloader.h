#ifndef PROPELLERLOADER_H
#define PROPELLERLOADER_H

#include <stdint.h>
#include <QObject>
#include <QByteArray>
#include "propellerconnection.h"
#include "loadelf.h"

enum LoadType {
    ltNone = 0,
    ltDownloadAndRun = (1 << 0),
    ltDownloadAndProgramEeprom = (1 << 1),
    ltDownloadAndProgramEepromAndRun = ltDownloadAndRun | ltDownloadAndProgramEeprom
};

class PropellerLoader : public QObject
{
    Q_OBJECT

public:
    PropellerLoader(PropellerConnection &connection);
    ~PropellerLoader();
    int load(const char *fileName, LoadType loadType);
    int load(uint8_t *image, int size, LoadType loadType);

private:
    uint8_t *loadSpinBinaryFile(FILE *fp, int *pLength);
    uint8_t *loadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize);
    static void generateIdentifyPacket(QByteArray &packet);
    static void generateLoaderPacket(QByteArray &packet, const uint8_t *image, int imageSize);
    static void encodeBytes(QByteArray &packet, const uint8_t *inBytes, int inCount);

    PropellerConnection &m_connection;
    int m_baudRate;
};

#endif // PROPELLERLOADER_H
