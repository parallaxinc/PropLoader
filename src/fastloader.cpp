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
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(80000000.0 / m_connection->loaderBaudRate() + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(80000000.0 / m_connection->fastLoaderBaudRate() + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(1.5 * ClockSpeed / m_connection->fastLoaderBaudRate() - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc(2.0 * ClockSpeed / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, (int)trunc((2.0 * ClockSpeed / m_connection->fastLoaderBaudRate()) * (10.0 / 12.0) + 0.5));
    
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
    int loaderImageSize, result, i;
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
        
    /* load the second-stage loader using the propeller ROM protocol */
    printf("Loading second-stage loader\n");
    result = m_connection->loadImage(loaderImage, loaderImageSize, response, sizeof(response));
    free(loaderImage);
    if (result != 0)
        return -1;

    result = getLong(&response[0]);
    if (result != packetID) {
        printf("error: second-stage loader failed to start - packetID %d, result %d\n", packetID, result);
        return -1;
    }

    /* switch to the final baud rate */
    m_connection->setBaudRate(m_connection->fastLoaderBaudRate());
    
    /* open the transparent serial connection that will be used for the second-stage loader */
    if (m_connection->connect() != 0) {
        printf("error: failed to connect to target\n");
        return -1;
    }

    /* transmit the image */
    printf("Loading image"); fflush(stdout);
    while (imageSize > 0) {
        int size;
        if ((size = imageSize) > m_connection->maxDataSize())
            size = m_connection->maxDataSize();
        putchar('.'); fflush(stdout);
        if (transmitPacket(packetID, image, size, &result) != 0) {
            printf("error: transmitPacket failed\n");
            return -1;
        }
        if (result != packetID - 1) {
            printf("Unexpected result: expected %d, received %d\n", packetID - 1, result);
            return -1;
        }
        imageSize -= size;
        image += size;
        --packetID;
    }
    putchar('\n');
    
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
    if (transmitPacket(packetID, verifyRAM, sizeof(verifyRAM), &result) != 0) {
        printf("error: transmitPacket failed\n");
        return -1;
    }
    if (result != -checksum) {
        printf("Checksum error: expected %08x, got %08x\n", -checksum, result);
        return -1;
    }
    packetID = -checksum;
    
    if (loadType & ltDownloadAndProgram) {
        printf("Programming EEPROM\n");
        if (transmitPacket(packetID, programVerifyEEPROM, sizeof(programVerifyEEPROM), &result, 8000) != 0) {
            printf("error: transmitPacket failed\n");
            return -1;
        }
        if (result != -checksum*2) {
            printf("EEPROM programming error: expected %08x, got %08x\n", -checksum*2, result);
            return -1;
        }
        packetID = -checksum*2;
    }
    
    /* transmit the final launch packets */
    
    printf("Sending readyToLaunch packet\n");
    if (transmitPacket(packetID, readyToLaunch, sizeof(readyToLaunch), &result) != 0) {
        printf("error: transmitPacket failed\n");
        return -1;
    }
    if (result != packetID - 1) {
        printf("ReadyToLaunch failed: expected %08x, got %08x\n", packetID - 1, result);
        return -1;
    }
    --packetID;
    
    printf("Sending launchNow packet\n");
    if (transmitPacket(packetID, launchNow, sizeof(launchNow), NULL) != 0) {
        printf("error: transmitPacket failed\n");
        return -1;
    }
    
    /* return successfully */
    return 0;
}

int Loader::transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout)
{
    int packetSize = 2*sizeof(uint32_t) + payloadSize;
    uint8_t *packet, response[8];
    int retries, result;
    int32_t tag, rtag;
    
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
        printf("transmit packet %d - tag %08x, size %d\n", id, tag, packetSize);
        if (m_connection->sendData(packet, packetSize) != packetSize) {
            printf("error: transmitPacket %d failed - sendData\n", id);
            free(packet);
            return -1;
        }
    
        /* receive the response */
        if (pResult) {
            if (m_connection->receiveDataExactTimeout(response, sizeof(response), timeout) != sizeof(response))
                printf("error: transmitPacket %d failed - receiveDataExactTimeout\n", id);
            else if ((rtag = getLong(&response[4])) == tag) {
                if ((result = getLong(&response[0])) == id)
                    printf("transmitPacket %d failed: duplicate id\n", id);
                else {
                    *pResult = result;
                    free(packet);
                    return 0;
                }
            }
            else
                printf("transmitPacket %d failed: wrong tag %08x - expected %08x\n", id, rtag, tag);
        }
        
        /* don't wait for a result */
        else {
            free(packet);
            return 0;
        }
        printf("transmitPacket %d failed - retrying\n", id);
    }
    
    /* free the packet */
    free(packet);
    
    /* return timeout */
    printf("error: transmitPacket %d failed - timeout\n", id);
    return -1;
}

