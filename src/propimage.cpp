#include "propimage.h"
#include "proploader.h"

static uint8_t initialCallFrame[] = {0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xF9, 0xFF};

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
    return *(uint8_t *)&((SpinHdr *)m_imageData)->clkmode;
}

void PropImage::setClkMode(uint8_t clkMode)
{
    *(uint8_t *)&((SpinHdr *)m_imageData)->clkmode = clkMode;
}

int PropImage::validate()
{
    SpinHdr *hdr = (SpinHdr *)m_imageData;
    uint8_t fullImage[MAX_IMAGE_SIZE];

    // make sure the image is at least the size of a Spin header
    if (m_imageSize <= (int)sizeof(SpinHdr))
        return IMAGE_TRUNCATED;
        
    // make sure the file is big enough to contain all of the code
    if (m_imageSize < hdr->vbase)
        return IMAGE_TRUNCATED;
        
    // make sure the image isn't too large
    if (m_imageSize > MAX_IMAGE_SIZE)
        return IMAGE_TOO_LARGE;
        
    // make sure the code starts in the right place
    if (hdr->pbase != 0x0010)
        return IMAGE_CORRUPTED;

    // make a local full-sized copy of the image
    memset(fullImage, 0, sizeof(fullImage));
    memcpy(fullImage, m_imageData, m_imageSize);
    
    // make sure there is space for the initial call frame
    if (hdr->dbase > MAX_IMAGE_SIZE)
        return IMAGE_TOO_LARGE;

    // setup the initial call frame
    int callFrameStart = hdr->dbase - sizeof(initialCallFrame);
    memcpy(&fullImage[callFrameStart], initialCallFrame, sizeof(initialCallFrame));
        
    // verify the checksum
    uint8_t *p = fullImage;
    uint8_t chksum = 0;
    for (int cnt = MAX_IMAGE_SIZE; --cnt >= 0; )
        chksum += *p++;
    if (chksum != 0)
        return IMAGE_CORRUPTED;
        
    // make sure there is no data after the code
    uint16_t idx = hdr->vbase;
    while (idx < m_imageSize && (fullImage[idx] == 0 || (idx >= hdr->dbase - sizeof(initialCallFrame) && idx < hdr->dbase && fullImage[idx] == initialCallFrame[idx - (hdr->dbase - sizeof(initialCallFrame))])))
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
