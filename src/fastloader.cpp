#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.h"
#include "proploader.h"
#include "propimage.h"

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

double ClockSpeed = 80000000.0;

uint8_t *Loader::generateInitialLoaderImage(int clockSpeed, int clockMode, int packetID, int loaderBaudRate, int fastLoaderBaudRate, int *pLength)
{
    int initAreaOffset = sizeof(rawLoaderImage) + RAW_LOADER_INIT_OFFSET_FROM_END;
    double floatClockSpeed = (double)clockSpeed;
    uint8_t *loaderImage;
    int checksum, i;
    
    // Allocate space for the image
    if (!(loaderImage = (uint8_t *)malloc(sizeof(rawLoaderImage))))
        return NULL;

    // Make a copy of the loader template
    memcpy(loaderImage, rawLoaderImage, sizeof(rawLoaderImage));
    
    // patch the clock frequency and mode
    PropImage image(loaderImage, sizeof(rawLoaderImage));
    image.setClkFreq(clockSpeed);
    image.setClkMode(clockMode);

/*
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset +  0, ClockMode and $07);                                  {Booter's clock selection bits}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset +  4, Round(ClockSpeed / InitialBaud));                    {Initial Bit Time}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset +  8, Round(ClockSpeed / FinalBaud));                      {Final Bit Time}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 12, Round(((1.5 * ClockSpeed) / FinalBaud) - MaxRxSenseError));  {1.5x Final Bit Time minus maximum start bit sense error}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 16, 2 * ClockSpeed div (3 * 4));                         {Failsafe Timeout (seconds-worth of Loader's Receive loop iterations)}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 20, Round(2 * ClockSpeed / FinalBaud * 10 / 12));        {EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations)}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 24, Max(Round(ClockSpeed * SSSHTime), 14));              {Minimum EEPROM Start/Stop Condition setup/hold time (400 KHz = 1/0.6 µS); Minimum 14 cycles}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 28, Max(Round(ClockSpeed * SCLHighTime), 14));           {Minimum EEPROM SCL high time (400 KHz = 1/0.6 µS); Minimum 14 cycles}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 32, Max(Round(ClockSpeed * SCLLowTime), 26));            {Minimum EEPROM SCL low time (400 KHz = 1/1.3 µS); Minimum 26 cycles}
      PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 36, PacketID);                                           {First Expected Packet ID; total packet count}
*/

    // Clock mode
    SetHostInitializedValue(loaderImage, initAreaOffset +  0, clockMode & 0x07);

    // Initial Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(floatClockSpeed / loaderBaudRate + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(floatClockSpeed / fastLoaderBaudRate + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(1.5 * floatClockSpeed / fastLoaderBaudRate - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc(2.0 * floatClockSpeed / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, (int)trunc(2.0 * floatClockSpeed / fastLoaderBaudRate * 10.0 / 12.0 + 0.5));
    
    const double SSSHTime    = 0.0000006;
    const double SCLHighTime = 0.0000006;
    const double SCLLowTime  = 0.0000013;

    // Minimum EEPROM Start/Stop Condition setup/hold time (1/0.6 µs); [Min 14 cycles]
    int SSSHTicks = (int)trunc(floatClockSpeed * SSSHTime + 0.5);
    if (SSSHTicks < 14)
        SSSHTicks = 14;
    
    // Minimum EEPROM SCL high time (1/0.6 µs); [Min 14 cycles]
    int SCLHighTicks = (int)trunc(floatClockSpeed * SCLHighTime + 0.5);
    if (SCLHighTicks < 14)
        SCLHighTicks = 14;
    
    // Minimum EEPROM SCL low time (1/1.3 µs); [Min 26 cycles]
    int SCLLowTicks = (int)trunc(floatClockSpeed * SCLLowTime + 0.5);
    if (SCLLowTicks < 26)
        SCLLowTicks = 26;
    
    // Minimum EEPROM Start/Stop Condition setup/hold time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    SetHostInitializedValue(loaderImage, initAreaOffset + 24, SSSHTicks);

    // Minimum EEPROM SCL high time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    SetHostInitializedValue(loaderImage, initAreaOffset + 28, SCLHighTicks);
    
    // Minimum EEPROM SCL low time (400 KHz = 1/1.3 µS); Minimum 26 cycles
    SetHostInitializedValue(loaderImage, initAreaOffset + 32, SCLLowTicks);

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
        message("Failed to load image '%s'", file);
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
    int sts;
    
    // get the binary clock settings
    PropImage img((uint8_t *)image, imageSize); // shouldn't really modify image!
    int binaryClockSpeed = img.clkFreq();
    int binaryClockMode = img.clkMode();
    
    // get the fast loader and program clock speeds
    int fastLoaderClockSpeed, clockSpeed;
    int gotFastLoaderClockSpeed = GetNumericConfigField(m_connection->config(), "fast-loader-clkfreq", &fastLoaderClockSpeed);
    int gotClockSpeed = GetNumericConfigField(m_connection->config(), "clkfreq", &clockSpeed);
    
    if (!gotFastLoaderClockSpeed) {
        if (gotClockSpeed)
            fastLoaderClockSpeed = clockSpeed;
        else
            fastLoaderClockSpeed = binaryClockSpeed;
    }
    if (gotClockSpeed) {
        img.setClkFreq(clockSpeed);
        img.updateChecksum();
    }

    // get the fast loader and program clock modes
    int fastLoaderClockMode, clockMode;
    int gotFastLoaderClockMode = GetNumericConfigField(m_connection->config(), "fast-loader-clkmode", &fastLoaderClockMode);
    int gotClockMode = GetNumericConfigField(m_connection->config(), "clkmode", &clockMode);
    
    if (!gotFastLoaderClockMode) {
        if (gotClockMode)
            fastLoaderClockMode = clockMode;
        else
            fastLoaderClockMode = binaryClockMode;
    }
    if (gotClockMode) {
        img.setClkMode(clockMode);
        img.updateChecksum();
    }
        
    message("fastLoaderClockSpeed %d, fastLoadClockMode %02x, clockSpeed %d, clockMode %02x",
            fastLoaderClockSpeed,
            fastLoaderClockMode,
            gotClockSpeed ? clockSpeed : binaryClockSpeed,
            gotClockMode ? clockMode : binaryClockMode);
    
    // get the loader baudrates
    int loaderBaudRate, fastLoaderBaudRate;
    if (!GetNumericConfigField(m_connection->config(), "loader-baud-rate", &loaderBaudRate))
        loaderBaudRate = DEF_LOADER_BAUDRATE;
    if (!GetNumericConfigField(m_connection->config(), "fast-loader-baud-rate", &fastLoaderBaudRate))
        fastLoaderBaudRate = DEF_FAST_LOADER_BAUDRATE;

    for (;;) {
        if ((sts = fastLoadImageHelper(image, imageSize, loadType, fastLoaderClockSpeed, fastLoaderClockMode, loaderBaudRate, fastLoaderBaudRate)) == 0)
            return 0;
        else if (sts == -2) {
            if ((fastLoaderBaudRate /= 2) >= 115200)
                nmessage(INFO_STEPPING_DOWN_BAUD_RATE, fastLoaderBaudRate);
            else
                break;
        }
        else
            return sts;
    }
        
    /* try a slow load if all baud rates failed */
    nmessage(INFO_USING_SINGLE_STAGE_LOADER);
    return m_connection->loadImage(image, imageSize, loadType, true);
}

/* returns:
    0 for success
    -1 for fatal errors
    -2 for errors where a lower baud rate might help
*/
int Loader::fastLoadImageHelper(const uint8_t *image, int imageSize, LoadType loadType, int clockSpeed, int clockMode, int loaderBaudRate, int fastLoaderBaudRate)
{
    uint8_t *loaderImage, response[8];
    int loaderImageSize, remaining, result, sts, i;
    int32_t packetID, checksum;
    SpinHdr *hdr = (SpinHdr *)image;

    // don't need to load beyond this even for .eeprom images
    imageSize = hdr->vbase;
    
    /* compute the image checksum */
    checksum = 0;
    for (i = 0; i < imageSize; ++i)
        checksum += image[i];
    for (i = 0; i < (int)sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];
    
    /* compute the packet ID (number of packets to be sent) */
    packetID = (imageSize + m_connection->maxDataSize() - 1) / m_connection->maxDataSize();

    /* generate a loader image */
    loaderImage = generateInitialLoaderImage(clockSpeed, clockMode, packetID, loaderBaudRate, fastLoaderBaudRate, &loaderImageSize);
    if (!loaderImage) {
        message("generateInitialLoaderImage failed");
        nerror(ERROR_INTERNAL_CODE_ERROR);
        return -1;
    }
        
    /* load the second-stage loader using the Propeller ROM protocol */
    message("Delivering second-stage loader");
    result = m_connection->loadImage(loaderImage, loaderImageSize, response, sizeof(response));
    free(loaderImage);
    if (result != 0)
        return result;

    result = getLong(&response[0]);
    if (result != packetID) {
        message("Second-stage loader failed to start - packetID %d, result %d", packetID, result);
        return -2;
    }

    /* switch to the final baud rate */
    m_connection->setBaudRate(fastLoaderBaudRate);
    
    /* open the transparent serial connection that will be used for the second-stage loader */
    if (m_connection->connect() != 0) {
        message("Failed to connect to target");
        nerror(ERROR_COMMUNICATION_LOST);
        return -1;
    }

    /* transmit the image */
    nmessage(INFO_DOWNLOADING, m_connection->portName());
    remaining = imageSize;
    while (remaining > 0) {
        int size;
        nprogress(INFO_BYTES_REMAINING, (long)remaining);
        if ((size = remaining) > m_connection->maxDataSize())
            size = m_connection->maxDataSize();
        if ((sts = transmitPacket(packetID, image, size, &result)) != 0)
            return -2;
        if (result != packetID - 1) {
            message("Unexpected response: expected %d, received %d", packetID - 1, result);
            return -2;
        }
        remaining -= size;
        image += size;
        --packetID;
    }
    nmessage(INFO_BYTES_SENT, (long)imageSize);
    
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
    nmessage(INFO_VERIFYING_RAM);
    if ((sts = transmitPacket(packetID, verifyRAM, sizeof(verifyRAM), &result)) != 0)
        return sts;
    if (result != -checksum) {
        nmessage(ERROR_RAM_CHECKSUM_FAILED);
        return -1;
    }
    packetID = -checksum;
    
    if (loadType & ltDownloadAndProgram) {
        nmessage(INFO_PROGRAMMING_EEPROM);
        if ((sts = transmitPacket(packetID, programVerifyEEPROM, sizeof(programVerifyEEPROM), &result, 8000)) != 0)
            return sts;
        if (result != -checksum*2) {
            nmessage(ERROR_EEPROM_CHECKSUM_FAILED);
            return -1;
        }
        packetID = -checksum*2;
    }
    
    /* transmit the final launch packets */
    
    message("Sending readyToLaunch packet");
    if ((sts = transmitPacket(packetID, readyToLaunch, sizeof(readyToLaunch), &result)) != 0)
        return sts;
    if (result != packetID - 1) {
        message("ReadyToLaunch failed: expected %08x, got %08x", packetID - 1, result);
        return -1;
    }
    --packetID;
    
    message("Sending launchNow packet");
    if ((sts = transmitPacket(packetID, launchNow, sizeof(launchNow), NULL)) != 0)
        return sts;
    
    /* return successfully */
    return 0;
}

/* returns:
    0 for success
    -1 for fatal errors
    -2 for errors where a lower baud rate might help
*/
int Loader::transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout)
{
    int packetSize = 2*sizeof(uint32_t) + payloadSize;
    uint8_t *packet, response[8];
    int retries, result;
    int32_t tag, rtag;
    
    /* build the packet to transmit */
    if (!(packet = (uint8_t *)malloc(packetSize))) {
        nmessage(ERROR_INSUFFICIENT_MEMORY);
        return -1;
    }
    setLong(&packet[0], id);
    memcpy(&packet[8], payload, payloadSize);
    
    /* send the packet */
    retries = 3;
    while (--retries >= 0) {
    
        /* setup the packet header */
#ifdef __MINGW32__
        tag = (int32_t)rand() | ((int32_t)rand() << 16);
#else
        tag = (int32_t)rand();
#endif
        setLong(&packet[4], tag);
        //printf("transmit packet %d - tag %08x, size %d\n", id, tag, packetSize);
        if (m_connection->sendData(packet, packetSize) != packetSize) {
            nmessage(ERROR_INTERNAL_CODE_ERROR);
            free(packet);
            return -1;
        }
    
        /* receive the response */
        if (pResult) {
            if (m_connection->receiveDataExactTimeout(response, sizeof(response), timeout) != sizeof(response))
                message("transmitPacket %d failed - receiveDataExactTimeout", id);
            else if ((rtag = getLong(&response[4])) == tag) {
                if ((result = getLong(&response[0])) == id)
                    message("transmitPacket %d failed: duplicate id", id);
                else {
                    *pResult = result;
                    free(packet);
                    return 0;
                }
            }
            else
                message("transmitPacket %d failed: wrong tag %08x - expected %08x", id, rtag, tag);
        }
        
        /* don't wait for a result */
        else {
            free(packet);
            return 0;
        }
        message("transmitPacket %d failed - retrying", id);
    }
    
    /* free the packet */
    free(packet);
    
    /* return timeout */
    message("transmitPacket %d failed - timeout", id);
    return -1;
}

