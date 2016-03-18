#ifndef PROPELLERIMAGE_H
#define PROPELLERIMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "loadelf.h"

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

/* spin object file header */
typedef struct {
    uint32_t clkfreq;
    uint8_t clkmode;
    uint8_t chksum;
    uint16_t pbase;
    uint16_t vbase;
    uint16_t dbase;
    uint16_t pcurr;
    uint16_t dcurr;
} SpinHdr;

/* spin object */
typedef struct {
    uint16_t next;
    uint8_t pubcnt;
    uint8_t objcnt;
    uint16_t pcurr;
    uint16_t numlocals;
} SpinObj;

class PropImage
{
public:
    PropImage();
    PropImage(uint8_t *imageData, int imageSize);
    ~PropImage();
    int load(const char *file);
    int setImage(uint8_t *imageData, int imageSize);
    void free();
    void updateChecksum();
    uint8_t *imageData() { return m_imageData; }
    int imageSize() { return m_imageSize; }
    uint32_t clkFreq();
    void setClkFreq(uint32_t clkFreq);
    uint8_t clkMode();
    void setClkMode(uint8_t clkMode);

private:
    int loadSpinBinaryFile(FILE *fp);
    int loadElfFile(FILE *fp, ElfHdr *hdr);
    static uint16_t getWord(const uint8_t *buf);
    static void setWord(uint8_t *buf, uint16_t value);
    static uint32_t getLong(const uint8_t *buf);
    static void setLong(uint8_t *buf, uint32_t value);

    uint8_t *m_imageData;
    int m_imageSize;
};

#endif // PROPELLERIMAGE_H
