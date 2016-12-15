#include "propimage.h"
#include "proploader.h"

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
}

void PropImage::setImage(uint8_t *imageData, int imageSize)
{
    m_imageData = imageData;
    m_imageSize = imageSize;
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

int PropImage::validate()
{
    // make sure the image is at least the size of a Spin header
    if (m_imageSize <= (int)sizeof(SpinHdr))
        return IMAGE_TRUNCATED;

    // verify the checksum
    uint8_t *p = m_imageData;
    uint8_t chksum = SPIN_STACK_FRAME_CHECKSUM;
    for (int cnt = m_imageSize; --cnt >= 0; )
        chksum += *p++;
    if (chksum != 0)
        return IMAGE_CORRUPTED;
        
    // make sure there is no data after the code
    SpinHdr *hdr = (SpinHdr *)m_imageData;
    uint16_t idx = hdr->vbase;
    while (idx < m_imageSize && idx < hdr->dbase && m_imageData[idx] == 0)
        ++idx;
    if (idx < m_imageSize)
        return IMAGE_CORRUPTED;

    // image is okay
    return 0;
}

int PropImage::validate(uint8_t *imageData, int imageSize)
{
    PropImage image(imageData, imageSize);
    return image.validate();
}

void PropImage::updateChecksum()
{
    SpinHdr *spinHdr = (SpinHdr *)m_imageData;
    uint8_t *p = m_imageData;
    uint8_t chksum;
    int cnt;
    spinHdr->chksum = 0;
    chksum = SPIN_STACK_FRAME_CHECKSUM;
    for (cnt = m_imageSize; --cnt >= 0; )
        chksum += *p++;
    spinHdr->chksum = -chksum;
}

void PropImage::updateChecksum(uint8_t *imageData, int imageSize)
{
    PropImage image(imageData, imageSize);
    image.updateChecksum();
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
