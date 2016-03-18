#include "propimage.h"

PropImage::PropImage()
    : m_imageData(NULL)
{
}

PropImage::PropImage(uint8_t *imageData, int imageSize)
    : m_imageData(NULL)
{
    setImage(imageData, imageSize);
}

PropImage::~PropImage()
{
    free();
}

int PropImage::setImage(uint8_t *imageData, int imageSize)
{
    free();
    if (!(m_imageData = (uint8_t *)malloc(imageSize)))
        return -1;
    memcpy(m_imageData, imageData, imageSize);
    m_imageSize = imageSize;
    return 0;
}

void PropImage::free()
{
    if (m_imageData) {
        ::free(m_imageData);
        m_imageData = NULL;
    }
}

uint32_t PropImage::clkFreq()
{
    return getLong((uint8_t *)&((SpinHdr *)m_imageData)->clkfreq);
}

void PropImage::setClkFreq(uint32_t clkFreq)
{
    setLong((uint8_t *)&((SpinHdr *)m_imageData)->clkfreq, clkFreq);
}

uint8_t PropImage::clkMode()
{
    return *(uint8_t *)&((SpinHdr *)m_imageData)->clkfreq;
}

void PropImage::setClkMode(uint8_t clkMode)
{
    *(uint8_t *)&((SpinHdr *)m_imageData)->clkfreq = clkMode;
}

void PropImage::updateChecksum()
{
    SpinHdr *spinHdr = (SpinHdr *)m_imageData;
    uint8_t *p = m_imageData;
    int chksum, cnt;
    spinHdr->chksum = chksum = 0;
    for (cnt = m_imageSize; --cnt >= 0; )
        chksum += *p++;
    spinHdr->chksum = SPIN_TARGET_CHECKSUM - chksum;
}

int PropImage::load(const char *file)
{
    ElfHdr elfHdr;
    FILE *fp;

    /* open the binary */
    if (!(fp = fopen(file, "rb"))) {
        printf("error: can't open '%s'\n", file);
        return -1;
    }

    /* check for an elf file */
    if (ReadAndCheckElfHdr(fp, &elfHdr)) {
        if (loadElfFile(fp, &elfHdr) != 0)
            return -1;
        fclose(fp);
    }
    else {
        if (loadSpinBinaryFile(fp) != 0)
            return -1;
        fclose(fp);
    }

    /* return successfully */
    return 0;
}

int PropImage::loadSpinBinaryFile(FILE *fp)
{
    /* free any existing image */
    free();

    /* get the size of the binary file */
    fseek(fp, 0, SEEK_END);
    m_imageSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* allocate space for the file */
    if (!(m_imageData = (uint8_t *)malloc(m_imageSize)))
        return -1;

    /* read the entire image into memory */
    if ((int)fread(m_imageData, 1, m_imageSize, fp) != m_imageSize) {
        ::free(m_imageData);
        m_imageData = NULL;
        return -1;
    }

    /* return successfully */
    return 0;
}

int PropImage::loadElfFile(FILE *fp, ElfHdr *hdr)
{
    uint32_t start, imageSize, cogImagesSize;
    ElfProgramHdr program;
    SpinHdr *spinHdr;
    ElfContext *c;
    uint8_t *buf;
    int i;

    /* free any existing image */
    free();

    /* open the elf file */
    if (!(c = OpenElfFile(fp, hdr)))
        return -1;

    /* get the total size of the program */
    if (!GetProgramSize(c, &start, &imageSize, &cogImagesSize))
        goto fail;
    m_imageSize = (int)imageSize;

    /* cog images in eeprom are not allowed */
    if (cogImagesSize > 0)
        goto fail;

    /* allocate a buffer big enough for the entire image */
    if (!(m_imageData = (uint8_t *)malloc(m_imageSize)))
        goto fail;
    memset(m_imageData, 0, m_imageSize);

    /* load each program section */
    for (i = 0; i < c->hdr.phnum; ++i) {
        if (!LoadProgramTableEntry(c, i, &program)
        ||  !(buf = LoadProgramSegment(c, &program))) {
            free();
            goto fail;
        }
        if (program.paddr < COG_DRIVER_IMAGE_BASE)
            memcpy(&m_imageData[program.paddr - start], buf, program.filesz);
    }

    /* free the elf file context */
    FreeElfContext(c);

    /* fixup the spin binary header */
    spinHdr = (SpinHdr *)m_imageData;
    setWord((uint8_t *)&spinHdr->vbase, m_imageSize);
    setWord((uint8_t *)&spinHdr->dbase, m_imageSize + 2 * sizeof(uint32_t)); // stack markers
    setWord((uint8_t *)&spinHdr->dcurr, spinHdr->dbase + sizeof(uint32_t));
    updateChecksum();

    /* return successfully */
    return 0;

fail:
    /* return failure */
    FreeElfContext(c);
    return -1;
}

uint16_t PropImage::getWord(const uint8_t *buf)
{
     return (buf[1] << 8) | buf[0];
}

void PropImage::setWord(uint8_t *buf, uint16_t value)
{
     buf[1] = value >>  8;
     buf[0] = value;
}

uint32_t PropImage::getLong(const uint8_t *buf)
{
     return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

void PropImage::setLong(uint8_t *buf, uint32_t value)
{
     buf[3] = value >> 24;
     buf[2] = value >> 16;
     buf[1] = value >>  8;
     buf[0] = value;
}
