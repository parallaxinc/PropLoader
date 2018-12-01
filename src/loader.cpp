#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.h"
#include "loadelf.h"
#include "propimage.h"
#include "proploader.h"

int Loader::loadFile(const char *file, LoadType loadType)
{
    uint8_t *image;
    int imageSize;
    int sts;
    
    /* make sure the image was loaded into memory */
    if (!(image = readFile(file, &imageSize)))
        return -1;
    
    /* load the file */
    sts = loadImage(image, imageSize, loadType);
    free(image);
    
    /* return load result */
    return sts;
}

int Loader::loadImage(const uint8_t *image, int imageSize, LoadType loadType)
{
    // get the binary clock settings
    PropImage img((uint8_t *)image, imageSize); // shouldn't really modify image!
    
    // get the fast loader and program clock speeds
    int clockSpeed;
    int gotClockSpeed = GetNumericConfigField(m_connection->config(), "clkfreq", &clockSpeed);
    if (gotClockSpeed) {
        img.setClkFreq(clockSpeed);
        img.updateChecksum();
    }

    // get the fast loader and program clock modes
    int clockMode;
    int gotClockMode = GetNumericConfigField(m_connection->config(), "clkmode", &clockMode);
    if (gotClockMode) {
        img.setClkMode(clockMode);
        img.updateChecksum();
    }
        
    nmessage(INFO_DOWNLOADING, m_connection->portName());
    return m_connection->loadImage(image, imageSize, loadType);
}

uint8_t *Loader::readFile(const char *file, int *pImageSize)
{
    uint8_t *image;
    int imageSize;
    ElfHdr elfHdr;
    FILE *fp;

    /* open the binary file */
    if (!(fp = fopen(file, "rb")))
        return NULL;
    
    /* check for an elf file */
    if (ReadAndCheckElfHdr(fp, &elfHdr))
        image = readElfFile(fp, &elfHdr, &imageSize);

    /* otherwise, assume a Spin binary */
    else
        image = readSpinBinaryFile(fp, &imageSize);
        
    /* close the binary file */
    fclose(fp);

    /* return the image */
    if (image) *pImageSize = imageSize;
    return image;
}

uint8_t *Loader::readSpinBinaryFile(FILE *fp, int *pImageSize)
{
    uint8_t *image;
    int imageSize;
    
    /* get the size of the binary file */
    fseek(fp, 0, SEEK_END);
    imageSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* allocate space for the file */
    if (!(image = (uint8_t *)malloc(imageSize)))
        return NULL;
    
    /* read the entire image into memory */
    if ((int)fread(image, 1, imageSize, fp) != imageSize) {
        free(image);
        return NULL;
    }

    /* return the buffer containing the file contents */
    *pImageSize = imageSize;
    return image;
}

uint8_t *Loader::readElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize)
{
    uint32_t start, imageSize, cogImagesSize;
    ElfProgramHdr program;
    uint8_t *image = NULL, *buf;
    SpinHdr *spinHdr;
    ElfContext *c;
    int i;

    /* open the elf file */
    if (!(c = OpenElfFile(fp, hdr)))
        return NULL;
        
    /* get the total size of the program */
    if (!GetProgramSize(c, &start, &imageSize, &cogImagesSize))
        goto fail;
        
    /* cog images in eeprom are not allowed */
    if (cogImagesSize > 0)
        goto fail;
    
    /* allocate a buffer big enough for the entire image */
    if (!(image = (uint8_t *)malloc(imageSize))) 
        goto fail;
    memset(image, 0, imageSize);
        
    /* load each program section */
    for (i = 0; i < c->hdr.phnum; ++i) {
        if (!LoadProgramTableEntry(c, i, &program)
        ||  !(buf = LoadProgramSegment(c, &program))) {
            free(image);
            goto fail;
        }
        if (program.paddr < COG_DRIVER_IMAGE_BASE)
            memcpy(&image[program.paddr - start], buf, program.filesz);
    }
    
    /* free the elf file context */
    FreeElfContext(c);
    
    /* fixup the spin binary header */
    spinHdr = (SpinHdr *)image;
    spinHdr->vbase = imageSize;
    spinHdr->dbase = imageSize + 2 * sizeof(uint32_t); // stack markers
    spinHdr->dcurr = spinHdr->dbase + sizeof(uint32_t);

    /* update the checksum */
    PropImage::updateChecksum(image, imageSize);

    /* return the image */
    *pImageSize = imageSize;
    return image;
    
fail:
    /* return failure */
    if (image)
        free(image);
    FreeElfContext(c);
    return NULL;
}

