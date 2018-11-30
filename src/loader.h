#ifndef __LOADER_H__
#define __LOADER_H__

#include <stdint.h>
#include <unistd.h>
#include "propconnection.h"
#include "loadelf.h"

class Loader {
public:
    Loader() : m_connection(0) {}
    Loader(PropConnection *connection) : m_connection(connection) {}
    ~Loader() {}
    void setConnection(PropConnection *connection) { m_connection = connection; }
    int identify(int *pVersion);
    int loadFile(const char *file, LoadType loadType = ltDownloadAndRun);
    int fastLoadFile(const char *file, LoadType loadType = ltDownloadAndRun);
    int loadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    int fastLoadImage(const uint8_t *image, int imageSize, LoadType loadType = ltDownloadAndRun);
    static uint8_t *readFile(const char *file, int *pImageSize);
private:
    int fastLoadImageHelper(const uint8_t *image, int imageSize, LoadType loadType, int clockSpeed, int clockMode, int loaderBaudRate, int fastLoaderBaudRate);
    uint8_t *generateInitialLoaderImage(int clockSpeed, int clockMode, int packetID, int loaderBaudRate, int fastLoaderBaudRate, int *pLength);
    int transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout = 2000);
    static uint8_t *readSpinBinaryFile(FILE *fp, int *pImageSize);
    static uint8_t *readElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize);
    PropConnection *m_connection;
};

inline void msleep(int ms)
{
    usleep(ms * 1000);
}

#endif
