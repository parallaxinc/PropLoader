/* Propeller WiFi loader

  Based on Jeff Martin's Pascal loader and Mike Westerfield's iOS loader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "serialpropconnection.h"
#include "loader.h"
#include "proploader.h"

#define MAX_BUFFER_SIZE         32768   /* The maximum buffer size. (BUG: git rid of this magic number) */
#define LENGTH_FIELD_SIZE       11      /* number of bytes in the length field */

// Propeller Download Stream Translator array.  Index into this array using the "Binary Value" (usually 5 bits) to translate,
// the incoming bit size (again, usually 5), and the desired data element to retrieve (encoding = translation, bitCount = bit count
// actually translated.

// first index is the next 1-5 bits from the incoming bit stream
// second index is the number of bits in the first value
// the result is a structure containing the byte to output to encode some or all of the input bits
static struct {
    uint8_t encoding;   // encoded byte to output
    uint8_t bitCount;   // number of bits encoded by the output byte
} PDSTx[32][5] =

//  ***  1-BIT  ***        ***  2-BIT  ***        ***  3-BIT  ***        ***  4-BIT  ***        ***  5-BIT  ***
{ { /*%00000*/ {0xFE, 1},  /*%00000*/ {0xF2, 2},  /*%00000*/ {0x92, 3},  /*%00000*/ {0x92, 3},  /*%00000*/ {0x92, 3} },
  { /*%00001*/ {0xFF, 1},  /*%00001*/ {0xF9, 2},  /*%00001*/ {0xC9, 3},  /*%00001*/ {0xC9, 3},  /*%00001*/ {0xC9, 3} },
  {            {0,    0},  /*%00010*/ {0xFA, 2},  /*%00010*/ {0xCA, 3},  /*%00010*/ {0xCA, 3},  /*%00010*/ {0xCA, 3} },
  {            {0,    0},  /*%00011*/ {0xFD, 2},  /*%00011*/ {0xE5, 3},  /*%00011*/ {0x25, 4},  /*%00011*/ {0x25, 4} },
  {            {0,    0},             {0,    0},  /*%00100*/ {0xD2, 3},  /*%00100*/ {0xD2, 3},  /*%00100*/ {0xD2, 3} },
  {            {0,    0},             {0,    0},  /*%00101*/ {0xE9, 3},  /*%00101*/ {0x29, 4},  /*%00101*/ {0x29, 4} },
  {            {0,    0},             {0,    0},  /*%00110*/ {0xEA, 3},  /*%00110*/ {0x2A, 4},  /*%00110*/ {0x2A, 4} },
  {            {0,    0},             {0,    0},  /*%00111*/ {0xFA, 3},  /*%00111*/ {0x95, 4},  /*%00111*/ {0x95, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01000*/ {0x92, 3},  /*%01000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01001*/ {0x49, 4},  /*%01001*/ {0x49, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01010*/ {0x4A, 4},  /*%01010*/ {0x4A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01011*/ {0xA5, 4},  /*%01011*/ {0xA5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01100*/ {0x52, 4},  /*%01100*/ {0x52, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01101*/ {0xA9, 4},  /*%01101*/ {0xA9, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01110*/ {0xAA, 4},  /*%01110*/ {0xAA, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01111*/ {0xD5, 4},  /*%01111*/ {0xD5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10001*/ {0xC9, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10010*/ {0xCA, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10011*/ {0x25, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10100*/ {0xD2, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10101*/ {0x29, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10110*/ {0x2A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10111*/ {0x95, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11001*/ {0x49, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11010*/ {0x4A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11011*/ {0xA5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11100*/ {0x52, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11101*/ {0xA9, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11110*/ {0xAA, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11111*/ {0x55, 5} }
 };

// After reset, the Propeller's exact clock rate is not known by either the host or the Propeller itself, so communication
// with the Propeller takes place based on a host-transmitted timing template that the Propeller uses to read the stream
// and generate the responses.  The host first transmits the 2-bit timing template, then transmits a 250-bit Tx handshake,
// followed by 250 timing templates (one for each Rx handshake bit expected) which the Propeller uses to properly transmit
// the Rx handshake sequence.  Finally, the host transmits another eight timing templates (one for each bit of the
// Propeller's version number expected) which the Propeller uses to properly transmit it's 8-bit hardware/firmware version
// number.
//
// After the Tx Handshake and Rx Handshake are properly exchanged, the host and Propeller are considered "connected," at
// which point the host can send a download command followed by image size and image data, or simply end the communication.
//
// PROPELLER HANDSHAKE SEQUENCE: The handshake (both Tx and Rx) are based on a Linear Feedback Shift Register (LFSR) tap
// sequence that repeats only after 255 iterations.  The generating LFSR can be created in Pascal code as the following function
// (assuming FLFSR is pre-defined Byte variable that is set to ord('P') prior to the first call of IterateLFSR).  This is
// the exact function that was used in previous versions of the Propeller Tool and Propellent software.
//
//      function IterateLFSR: Byte;
//      begin //Iterate LFSR, return previous bit 0
//      Result := FLFSR and 0x01;
//      FLFSR := FLFSR shl 1 and 0xFE or (FLFSR shr 7 xor FLFSR shr 5 xor FLFSR shr 4 xor FLFSR shr 1) and 1;
//      end;
//
// The handshake bit stream consists of the lowest bit value of each 8-bit result of the LFSR described above.  This LFSR
// has a domain of 255 combinations, but the host only transmits the first 250 bits of the pattern, afterwards, the Propeller
// generates and transmits the next 250-bits based on continuing with the same LFSR sequence.  In this way, the host-
// transmitted (host-generated) stream ends 5 bits before the LFSR starts repeating the initial sequence, and the host-
// received (Propeller generated) stream that follows begins with those remaining 5 bits and ends with the leading 245 bits
// of the host-transmitted stream.
//
// For speed and compression reasons, this handshake stream has been encoded as tightly as possible into the pattern
// described below.
//
// The TxHandshake array consists of 209 bytes that are encoded to represent the required '1' and '0' timing template bits,
// 250 bits representing the lowest bit values of 250 iterations of the Propeller LFSR (seeded with ASCII 'P'), 250 more
// timing template bits to receive the Propeller's handshake response, and more to receive the version.
static uint8_t txHandshake[] = {
    // First timing template ('1' and '0') plus first two bits of handshake ('0' and '1').
    0x49,
    // Remaining 248 bits of handshake...
    0xAA,0x52,0xA5,0xAA,0x25,0xAA,0xD2,0xCA,0x52,0x25,0xD2,0xD2,0xD2,0xAA,0x49,0x92,
    0xC9,0x2A,0xA5,0x25,0x4A,0x49,0x49,0x2A,0x25,0x49,0xA5,0x4A,0xAA,0x2A,0xA9,0xCA,
    0xAA,0x55,0x52,0xAA,0xA9,0x29,0x92,0x92,0x29,0x25,0x2A,0xAA,0x92,0x92,0x55,0xCA,
    0x4A,0xCA,0xCA,0x92,0xCA,0x92,0x95,0x55,0xA9,0x92,0x2A,0xD2,0x52,0x92,0x52,0xCA,
    0xD2,0xCA,0x2A,0xFF,
    // 250 timing templates ('1' and '0') to receive 250-bit handshake from Propeller.
    // This is encoded as two pairs per byte; 125 bytes.
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    // 8 timing templates ('1' and '0') to receive 8-bit Propeller version; two pairs per byte; 4 bytes.
    0x29,0x29,0x29,0x29};
    
// Shutdown command (0); 11 bytes.
static uint8_t shutdownCmd[] = {0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM and Run command (1); 11 bytes.
static uint8_t loadRunCmd[] = {0xc9, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM, Program EEPROM, and Shutdown command (2); 11 bytes.
static uint8_t programShutdownCmd[] = {0xca, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM, Program EEPROM, and Run command (3); 11 bytes.
static uint8_t programRunCmd[] = {0x25, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xfe};

// The RxHandshake array consists of 125 bytes encoded to represent the expected 250-bit (125-byte @ 2 bits/byte) response
// of continuing-LFSR stream bits from the Propeller, prompted by the timing templates following the TxHandshake stream.
static uint8_t rxHandshake[] = {
    0xEE,0xCE,0xCE,0xCF,0xEF,0xCF,0xEE,0xEF,0xCF,0xCF,0xEF,0xEF,0xCF,0xCE,0xEF,0xCF,
    0xEE,0xEE,0xCE,0xEE,0xEF,0xCF,0xCE,0xEE,0xCE,0xCF,0xEE,0xEE,0xEF,0xCF,0xEE,0xCE,
    0xEE,0xCE,0xEE,0xCF,0xEF,0xEE,0xEF,0xCE,0xEE,0xEE,0xCF,0xEE,0xCF,0xEE,0xEE,0xCF,
    0xEF,0xCE,0xCF,0xEE,0xEF,0xEE,0xEE,0xEE,0xEE,0xEF,0xEE,0xCF,0xCF,0xEF,0xEE,0xCE,
    0xEF,0xEF,0xEF,0xEF,0xCE,0xEF,0xEE,0xEF,0xCF,0xEF,0xCF,0xCF,0xCE,0xCE,0xCE,0xCF,
    0xCF,0xEF,0xCE,0xEE,0xCF,0xEE,0xEF,0xCE,0xCE,0xCE,0xEF,0xEF,0xCF,0xCF,0xEE,0xEE,
    0xEE,0xCE,0xCF,0xCE,0xCE,0xCF,0xCE,0xEE,0xEF,0xEE,0xEF,0xEF,0xCF,0xEF,0xCE,0xCE,
    0xEF,0xCE,0xEE,0xCE,0xEF,0xCE,0xCE,0xEE,0xCF,0xCF,0xCE,0xCF,0xCF};

/* EncodeBytes
    parameters:
        inBytes is a pointer to a buffer of bytes to be encoded
        inCount is the number of bytes in inBytes
        outBytes is a pointer to a buffer to receive the encoded bytes
        outSize is the size of the outBytes buffer
    returns the number of bytes written to the outBytes buffer or -1 if the encoded data does not fit
*/
static int EncodeBytes(const uint8_t *inBytes, int inCount, uint8_t *outBytes, int outSize)
{
    static uint8_t masks[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f };
    int bitCount = inCount * 8;
    int nextBit = 0;
    int outCount = 0;
    
    /* encode all bits in the input buffer */
    while (nextBit < bitCount) {
        int bits, bitsIn;
    
        /* encode 5 bits or whatever remains in inBytes, whichever is smaller */
        bitsIn = bitCount - nextBit;
        if (bitsIn > 5)
            bitsIn = 5;
            
        /* extract the next 'bitsIn' bits from the input buffer */
        bits = ((inBytes[nextBit / 8] >> (nextBit % 8)) | (inBytes[nextBit / 8 + 1] << (8 - (nextBit % 8)))) & masks[bitsIn];
    
        /* make sure there is enough space in the output buffer */
        if (outCount >= outSize)
            return -1;
            
        /* store the encoded value */
        outBytes[outCount++] = PDSTx[bits][bitsIn - 1].encoding;
        
        /* advance to the next group of bits */
        nextBit += PDSTx[bits][bitsIn - 1].bitCount;
    }
    
    /* return the number of encoded bytes */
    return outCount;
}

static uint8_t *GenerateIdentifyPacket(int *pLength)
{
    uint8_t *packet;
    int packetSize;
    
    /* determine the size of the packet */
    packetSize = sizeof(txHandshake) + sizeof(shutdownCmd);
    
    /* allocate space for the full packet */
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return NULL;
        
    /* copy the handshake image and the command to the packet */
    memcpy(packet, txHandshake, sizeof(txHandshake));
    memcpy(packet + sizeof(txHandshake), shutdownCmd, sizeof(shutdownCmd));
        
    /* return the packet and its length */
    *pLength = packetSize;
    return packet;
}

static uint8_t *GenerateLoaderPacket(const uint8_t *image, int imageSize, int *pLength, LoadType loadType)
{
    int imageSizeInLongs = (imageSize + 3) / 4;
    uint8_t encodedImage[MAX_BUFFER_SIZE * 8]; // worst case assuming one byte per bit encoding
    int encodedImageSize, packetSize, cmdLen, tmp, i;
    uint8_t *packet, *cmd, *p;
    
    /* encode the image */
    encodedImageSize = EncodeBytes(image, imageSize, encodedImage, sizeof(encodedImage));
    if (encodedImageSize < 0)
        return NULL;
    
    /* select command */
    switch (loadType) {
    case ltShutdown:
        cmd = shutdownCmd;
        cmdLen = sizeof(shutdownCmd);
        break;
    case ltDownloadAndRun:
        cmd = loadRunCmd;
        cmdLen = sizeof(loadRunCmd);
        break;
    case ltDownloadAndProgram:
        cmd = programShutdownCmd;
        cmdLen = sizeof(programShutdownCmd);
        break;
    case ltDownloadAndProgramAndRun:
        cmd = programRunCmd;
        cmdLen = sizeof(programRunCmd);
        break;
    default:
        return NULL;
    }
        
    /* determine the size of the packet */
    packetSize = sizeof(txHandshake) + cmdLen + LENGTH_FIELD_SIZE + encodedImageSize;
    
    /* allocate space for the full packet */
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return NULL;
        
    /* copy the handshake image and the command to the packet */
    memcpy(packet, txHandshake, sizeof(txHandshake));

    /* copy the command to the packet */
    memcpy(packet + sizeof(txHandshake), cmd, cmdLen);
    
    /* build the packet from the handshake data, the image length and the encoded image */
    p = packet + sizeof(txHandshake) + cmdLen;
    tmp = imageSizeInLongs;
    for (i = 0; i < LENGTH_FIELD_SIZE; ++i) {
        *p++ = 0x92 | (i == 10 ? 0x60 : 0x00) | (tmp & 1) | ((tmp & 2) << 2) | ((tmp & 4) << 4);
        tmp >>= 3;
    }
    memcpy(p, encodedImage, encodedImageSize);
    
    /* return the packet and its length */
    *pLength = packetSize;
    return packet;
}

int SerialPropConnection::identify(int *pVersion)
{
    uint8_t packet2[MAX_BUFFER_SIZE]; // must be at least as big as maxDataSize()
    int version, cnt, i;
    uint8_t *packet;
    int packetSize;
    
    /* generate the identify packet */
    if (!(packet = GenerateIdentifyPacket(&packetSize))) {
        message("Failed to generate identify packet");
        goto fail;
    }

    /* reset the Propeller */
    generateResetSignal();
    
    /* send the identify packet */
    sendData(packet, packetSize);
    
    /* send the verification packet (all timing templates) */
    memset(packet2, 0xF9, maxDataSize());
    sendData(packet2, maxDataSize());
    
    /* receive the handshake response and the hardware version */
    cnt = receiveDataExactTimeout(packet2, sizeof(rxHandshake) + 4, 2000);
    if (cnt < 0)
        goto fail;
    
    /* verify the handshake response */
    if (cnt != sizeof(rxHandshake) + 4 || memcmp(packet2, rxHandshake, sizeof(rxHandshake)) != 0) {
        message("Handshake failed");
        goto fail;
    }
    
    /* verify the hardware version */
    version = 0;
    for (i = sizeof(rxHandshake); i < cnt; ++i)
        version = ((version >> 2) & 0x3F) | ((packet2[i] & 0x01) << 6) | ((packet2[i] & 0x20) << 2);
    
    /* return successfully */
    *pVersion = version;
    return 0;
    
    /* return failure */
fail:
    disconnect();
    return -1;
}

/* returns:
    0 for success
    -1 for fatal errors
    -2 for errors where a lower baud rate might help
*/
int SerialPropConnection::loadImage(const uint8_t *image, int imageSize, uint8_t *response, int responseSize)
{
    if (loadImage(image, imageSize, ltDownloadAndRun) != 0)
        return -1;
    return receiveDataExactTimeout(response, responseSize, 1000) == responseSize ? 0 : -2;
}

#define ACK_POLLING_INTERVAL        10
#define RAM_PROGRAMMING_RETRIES     (10000 / ACK_POLLING_INTERVAL)
#define EEPROM_PROGRAMMING_RETRIES  (5000 / ACK_POLLING_INTERVAL)
#define EEPROM_VERIFY_RETRIES       (2000 / ACK_POLLING_INTERVAL)

int SerialPropConnection::loadImage(const uint8_t *image, int imageSize, LoadType loadType, int info)
{
    uint8_t packet2[MAX_BUFFER_SIZE]; // must be at least as big as maxDataSize()
    int packetSize, version, retries, cnt, i;
    int loaderBaudRate;
    uint8_t *packet;
    
    if (!GetNumericConfigField(config(), "loader-baud-rate", &loaderBaudRate))
        loaderBaudRate = DEF_LOADER_BAUDRATE;
        
    /* use the loader baud rate */
    if (setBaudRate(loaderBaudRate) != 0) {
        nerror(ERROR_FAILED_TO_SET_BAUD_RATE);
        return -1;
    }
        
    /* generate a loader packet */
    if (!(packet = GenerateLoaderPacket(image, imageSize, &packetSize, loadType))) {
        nerror(ERROR_INTERNAL_CODE_ERROR);
        return -1;
    }

    /* reset the Propeller */
    generateResetSignal();
    
    /* send the packet including the image */
    if (info)
        nmessage(INFO_DOWNLOADING, portName());
    sendData(packet, packetSize);
    if (info)
        nmessage(INFO_BYTES_SENT, (long)imageSize);
    free(packet);
    
    /* clock out the handshake response */
    memset(packet2, 0xF9, sizeof(rxHandshake) + 4);
    sendData(packet2, sizeof(rxHandshake) + 4);
    
    /* receive the handshake response and the hardware version */
    cnt = receiveDataExactTimeout(packet2, sizeof(rxHandshake) + 4, 2000);
    
    /* verify the handshake response */
    if (cnt != sizeof(rxHandshake) + 4 || memcmp(packet2, rxHandshake, sizeof(rxHandshake)) != 0) {
        nmessage(ERROR_PROPELLER_NOT_FOUND, portName());
        return -1;
    }
    
    /* verify the hardware version */
    version = 0;
    for (i = sizeof(rxHandshake); i < cnt; ++i)
        version = ((version >> 2) & 0x3F) | ((packet2[i] & 0x01) << 6) | ((packet2[i] & 0x20) << 2);
    if (version != 1) {
        nmessage(ERROR_WRONG_PROPELLER_VERSION, version);
        return -1;
    }
    
    if (info)
        nmessage(INFO_VERIFYING_RAM);

    /* receive the RAM verify response */
    packet2[0] = 0xF9;
    retries = RAM_PROGRAMMING_RETRIES;
    do {
        sendData(packet2, 1);
        cnt = receiveDataExactTimeout(packet2, 1, 10);
    } while (cnt <= 0 && --retries > 0);

    /* check for timeout */
    if (cnt <= 0) {
        nmessage(ERROR_COMMUNICATION_LOST);
        return -1;
    }
    
    /* verify the checksum response */
    if (packet2[0] != 0xFE) {
        //message("RAM checksum failed: expected 0xFE, got %02x", packet2[0]);
        nmessage(ERROR_RAM_CHECKSUM_FAILED);
        return -1;
    }
    
    /* handle EEPROM programming */
    if (loadType == ltDownloadAndProgram || loadType == ltDownloadAndProgramAndRun) {
    
        if (info)
            nmessage(INFO_PROGRAMMING_EEPROM);

        /* receive the EEPROM programming complete response */
        packet2[0] = 0xF9;
        retries = EEPROM_PROGRAMMING_RETRIES;
        do {
            sendData(packet2, 1);
            cnt = receiveDataExactTimeout(packet2, 1, 10);
        } while (cnt <= 0 && --retries > 0);

        /* check for timeout */
        if (cnt <= 0) {
            nmessage(ERROR_COMMUNICATION_LOST);
            return -1;
        }
    
        /* verify the checksum response */
        if (packet2[0] != 0xFE) {
            //message("EEPROM checksum failed: expected 0xFE, got %02x", packet2[0]);
            nmessage(ERROR_EEPROM_CHECKSUM_FAILED);
            return -1;
        }
    
        if (info)
            nmessage(INFO_VERIFYING_EEPROM);

        /* receive the EEPROM verify response */
        packet2[0] = 0xF9;
        retries = EEPROM_VERIFY_RETRIES;
        do {
            sendData(packet2, 1);
            cnt = receiveDataExactTimeout(packet2, 1, 10);
        } while (cnt <= 0 && --retries > 0);

        /* check for timeout */
        if (cnt <= 0) {
            message("Timeout waiting for checksum");
            nmessage(ERROR_COMMUNICATION_LOST);
            return -1;
        }
    
        /* verify the checksum response */
        if (packet2[0] != 0xFE) {
            //message("EEPROM verify failed: expected 0xFE, got %02x", packet2[0]);
            nmessage(ERROR_EEPROM_VERIFY_FAILED);
            return -1;
        }
    }
       
    /* return successfully */
    return 0;
}

