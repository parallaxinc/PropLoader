#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.h"

#define MAX_RX_SENSE_ERROR      23          /* Maximum number of cycles by which the detection of a start bit could be off (as affected by the Loader code) */

// Offset (in bytes) from end of Loader Image pointing to where most host-initialized values exist.
// Host-Initialized values are: Initial Bit Time, Final Bit Time, 1.5x Bit Time, Failsafe timeout,
// End of Packet timeout, and ExpectedID.  In addition, the image checksum at word 5 needs to be
// updated.  All these values need to be updated before the download stream is generated.
// NOTE: DAT block data is always placed before the first Spin method
#define RAW_LOADER_INIT_OFFSET_FROM_END (-(10 * 4) - 8)

// Raw loader image.  This is a memory image of a Propeller Application written in PASM that fits into our initial
// download packet.  Once started, it assists with the remainder of the download (at a faster speed and with more
// relaxed interstitial timing conducive of Internet Protocol delivery. This memory image isn't used as-is; before
// download, it is first adjusted to contain special values assigned by this host (communication timing and
// synchronization values) and then is translated into an optimized Propeller Download Stream understandable by the
// Propeller ROM-based boot loader.
#include "IP_Loader.h"

double ClockSpeed = 80000000.0;

static uint8_t initCallFrame[] = {0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xF9, 0xFF};

static void SetHostInitializedValue(uint8_t *bytes, int offset, int value)
{
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = (value >> (i * 8)) & 0xFF;
}

static int32_t getLong(const uint8_t *buf)
{
     return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

static void setLong(uint8_t *buf, uint32_t value)
{
     buf[3] = value >> 24;
     buf[2] = value >> 16;
     buf[1] = value >>  8;
     buf[0] = value;
}

uint8_t *Loader::generateInitialLoaderImage(int packetID, int *pLength)
{
    int initAreaOffset = sizeof(rawLoaderImage) + RAW_LOADER_INIT_OFFSET_FROM_END;
    uint8_t *loaderImage;
    int checksum, i;

    // Allocate space for the image
    if (!(loaderImage = (uint8_t *)malloc(sizeof(rawLoaderImage))))
        return NULL;

    // Make a copy of the loader template
    memcpy(loaderImage, rawLoaderImage, sizeof(rawLoaderImage));
    
    // Clock mode
    //SetHostInitializedValue(loaderImage, initAreaOffset +  0, 0);

    // Initial Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(80000000.0 / m_baudrate + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(80000000.0 / m_connection->finalBaudRate() + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(1.5 * ClockSpeed / m_connection->finalBaudRate() - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc(2.0 * ClockSpeed / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, (int)trunc((2.0 * ClockSpeed / m_connection->finalBaudRate()) * (10.0 / 12.0) + 0.5));
    
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 24, Max(Round(ClockSpeed * SSSHTime), 14));
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 28, Max(Round(ClockSpeed * SCLHighTime), 14));
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 32, Max(Round(ClockSpeed * SCLLowTime), 26));

    // Minimum EEPROM Start/Stop Condition setup/hold time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 24, 14);

    // Minimum EEPROM SCL high time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 28, 14);

    // Minimum EEPROM SCL low time (400 KHz = 1/1.3 µS); Minimum 26 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 32, 26);

    // First Expected Packet ID; total packet count.
    SetHostInitializedValue(loaderImage, initAreaOffset + 36, packetID);

    // Recalculate and update checksum so low byte of checksum calculates to 0.
    checksum = 0;
    loaderImage[5] = 0; // start with a zero checksum
    for (i = 0; i < (int)sizeof(rawLoaderImage); ++i)
        checksum += loaderImage[i];
    for (i = 0; i < (int)sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];
    loaderImage[5] = 256 - (checksum & 0xFF);
    
    /* return the loader image */
    *pLength = sizeof(rawLoaderImage);
    return loaderImage;
}

int Loader::fastLoadFile(const char *file, LoadType loadType)
{
    uint8_t *image;
    int imageSize;
    int sts;
    
    /* make sure the image was loaded into memory */
    if (!(image = readFile(file, &imageSize))) {
        printf("error: failed to load image '%s'\n", file);
        return -1;
    }
    
    /* load the file */
    sts = fastLoadImage(image, imageSize, loadType);
    free(image);
    
    /* return load result */
    return sts;
}

int Loader::fastLoadImage(const uint8_t *image, int imageSize, LoadType loadType)
{
    uint8_t *loaderImage, response[8];
    int loaderImageSize, result, cnt, i;
    int32_t packetID, checksum;
    
    /* compute the packet ID (number of packets to be sent) */
    packetID = (imageSize + m_connection->maxDataSize() - 1) / m_connection->maxDataSize();

    /* generate a loader image */
    loaderImage = generateInitialLoaderImage(packetID, &loaderImageSize);
    if (!loaderImage)
        return -1;
        
    /* compute the image checksum */
    checksum = 0;
    for (i = 0; i < imageSize; ++i)
        checksum += image[i];
    for (i = 0; i < (int)sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];
        
    /* open the transparent serial connection that will be used for the second-stage loader */
    m_connection->connect();

    /* load the second-stage loader using the propeller ROM protocol */
    printf("Loading second-stage loader\n");
    result = m_connection->loadImage(loaderImage, loaderImageSize, ltDownloadAndRun);
    free(loaderImage);
    if (result != 0)
        return -1;

    printf("Waiting for second-stage loader initial response\n");
    cnt = m_connection->receiveDataExactTimeout(response, sizeof(response), 2000);
    result = getLong(&response[0]);
    if (cnt != 8 || result != packetID) {
        printf("error: second-stage loader failed to start - cnt %d, packetID %d, result %d\n", cnt, packetID, result);
        return -1;
    }
    printf("Got initial second-stage loader response\n");
    
    /* switch to the final baud rate */
    m_connection->setBaudRate(m_connection->finalBaudRate());
    
    /* transmit the image */
    while (imageSize > 0) {
        int size;
        if ((size = imageSize) > m_connection->maxDataSize())
            size = m_connection->maxDataSize();
        printf("Transmitting packet %d\n", packetID);
        if (transmitPacket(packetID, image, size, &result) != 0) {
            printf("error: transmitPacket failed\n");
            return -1;
        }
        if (result != packetID - 1)
            printf("Unexpected result: expected %d, received %d\n", packetID - 1, result);
        imageSize -= size;
        image += size;
        --packetID;
    }
    
    /*
        When we're doing a download that does not include an EEPROM write, the Packet IDs end up as:

        ltVerifyRAM: zero
        ltReadyToLaunch: -Checksum
        ltLaunchNow: -Checksum - 1

        ... and when we're doing a download that includes an EEPROM write, the Packet IDs end up as:

        ltVerifyRAM: zero
        ltProgramEEPROM: -Checksum
        ltReadyToLaunch: -Checksum*2
        ltLaunchNow: -Checksum*2 - 1
    */
    
    /* transmit the RAM verify packet and verify the checksum */
    printf("Verifying RAM\n");
    transmitPacket(packetID, verifyRAM, sizeof(verifyRAM), &result);
    if (result != -checksum)
        printf("Checksum error: expected %08x, got %08x\n", -checksum, result);
    packetID = -checksum;
    
    if (loadType & ltDownloadAndProgram) {
        printf("Programming EEPROM\n");
        transmitPacket(packetID, programVerifyEEPROM, sizeof(programVerifyEEPROM), &result, 8000);
        if (result != -checksum*2)
            printf("EEPROM programming error: expected %08x, got %08x\n", -checksum*2, result);
        packetID = -checksum*2;
    }
    
    /* transmit the final launch packets */
    
    printf("Sending readyToLaunch packet\n");
    transmitPacket(packetID, readyToLaunch, sizeof(readyToLaunch), &result);
    if (result != packetID - 1)
        printf("ReadyToLaunch failed: expected %08x, got %08x\n", packetID - 1, result);
    --packetID;
    
    printf("Sending launchNow packet\n");
    transmitPacket(packetID, launchNow, sizeof(launchNow), NULL);
    
    /* return successfully */
    return 0;
}

int Loader::transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout)
{
    int packetSize = 2*sizeof(uint32_t) + payloadSize;
    uint8_t *packet, response[8];
    int retries, result, cnt;
    int32_t tag;
    
    /* build the packet to transmit */
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return -1;
    setLong(&packet[0], id);
    memcpy(&packet[8], payload, payloadSize);
    
    /* send the packet */
    retries = 3;
    while (--retries >= 0) {
    
        /* setup the packet header */
        tag = (int32_t)rand();
        setLong(&packet[4], tag);
        //printf("transmit packet %d\n", id);
        m_connection->sendData(packet, packetSize);
    
        /* receive the response */
        if (pResult) {
            cnt = m_connection->receiveDataExactTimeout(response, sizeof(response), timeout);
            result = getLong(&response[0]);
            if (cnt == 8 && getLong(&response[4]) == tag && result != id) {
                free(packet);
                *pResult = result;
                return 0;
            }
        }
        
        /* don't wait for a result */
        else {
            free(packet);
            return 0;
        }
    }
    
    /* free the packet */
    free(packet);
    
    /* return timeout */
    printf("error: transmitPacket %d failed\n", id);
    return -1;
}

