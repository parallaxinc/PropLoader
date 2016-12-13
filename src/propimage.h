#ifndef PROPELLERIMAGE_H
#define PROPELLERIMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "loadelf.h"

/* initial stack frame checksum for a binary file */
/* this value is the sum of the bytes in the initial stack frame which are not included in the .binary file.
   0xEC = (0xFF + 0xFF + 0xF9 + 0xFF + 0xFF + 0xFF + 0xF9 + 0xFF) & 0xFF
*/
#define SPIN_STACK_FRAME_CHECKSUM   0xEC

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
    enum {
        SUCCESS = 0,
        IMAGE_TRUNCATED = -1,
        IMAGE_CORRUPTED  = -2
    };
    PropImage();
    PropImage(uint8_t *imageData, int imageSize);
    ~PropImage();
    void setImage(uint8_t *imageData, int imageSize);
    int validate();
    void updateChecksum();
    uint8_t *imageData() { return m_imageData; }
    int imageSize() { return m_imageSize; }
    uint32_t clkFreq();
    void setClkFreq(uint32_t clkFreq);
    uint8_t clkMode();
    void setClkMode(uint8_t clkMode);
    static int validate(uint8_t *imageData, int imageSize);
    static void updateChecksum(uint8_t *imageData, int imageSize);

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
