/* Propeller WiFi loader

  Based on Jeff Martin's Pascal loader and Mike Westerfield's iOS loader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.hpp"

#define MAX_BUFFER_SIZE         4096        /* The maximum buffer size. (BUG: git rid of this magic number) */
#define FINAL_BAUD              921600		/* Final XBee-to-Propeller baud rate. */
#define MAX_RX_SENSE_ERROR      23			/* Maximum number of cycles by which the detection of a start bit could be off (as affected by the Loader code) */
#define LENGTH_FIELD_SIZE       11          /* number of bytes in the length field */

// Offset (in bytes) from end of Loader Image pointing to where most host-initialized values exist.
// Host-Initialized values are: Initial Bit Time, Final Bit Time, 1.5x Bit Time, Failsafe timeout,
// End of Packet timeout, and ExpectedID.  In addition, the image checksum at word 5 needs to be
// updated.  All these values need to be updated before the download stream is generated.
#define RAW_LOADER_INIT_OFFSET_FROM_END (-8*4)

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
//		function IterateLFSR: Byte;
//		begin //Iterate LFSR, return previous bit 0
//		Result := FLFSR and 0x01;
//		FLFSR := FLFSR shl 1 and 0xFE or (FLFSR shr 7 xor FLFSR shr 5 xor FLFSR shr 4 xor FLFSR shr 1) and 1;
//		end;
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
    0x29,0x29,0x29,0x29,
    // Download command (1; program RAM and run); 11 bytes.
    0x93,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0xF2};

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

static void SetHostInitializedValue(uint8_t *bytes, int offset, int value)
{
	for (int i = 0; i < 4; ++i)
        bytes[offset + i] = (value >> (i * 8)) & 0xFF;
}

static uint32_t getLong(const uint8_t *buf)
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

// Raw loader image.  This is a memory image of a Propeller Application written in PASM that fits into our initial
// download packet.  Once started, it assists with the remainder of the download (at a faster speed and with more
// relaxed interstitial timing conducive of Internet Protocol delivery. This memory image isn't used as-is; before
// download, it is first adjusted to contain special values assigned by this host (communication timing and
// synchronization values) and then is translated into an optimized Propeller Download Stream understandable by the
// Propeller ROM-based boot loader.
static uint8_t rawLoaderImage[] = {
    0x00,0xB4,0xC4,0x04,0x6F,0xC3,0x10,0x00,0x5C,0x01,0x64,0x01,0x54,0x01,0x68,0x01,
    0x4C,0x01,0x02,0x00,0x44,0x01,0x00,0x00,0x48,0xE8,0xBF,0xA0,0x48,0xEC,0xBF,0xA0,
    0x49,0xA0,0xBC,0xA1,0x01,0xA0,0xFC,0x28,0xF1,0xA1,0xBC,0x80,0xA0,0x9E,0xCC,0xA0,
    0x49,0xA0,0xBC,0xF8,0xF2,0x8F,0x3C,0x61,0x05,0x9E,0xFC,0xE4,0x04,0xA4,0xFC,0xA0,
    0x4E,0xA2,0xBC,0xA0,0x08,0x9C,0xFC,0x20,0xFF,0xA2,0xFC,0x60,0x00,0xA3,0xFC,0x68,
    0x01,0xA2,0xFC,0x2C,0x49,0xA0,0xBC,0xA0,0xF1,0xA1,0xBC,0x80,0x01,0xA2,0xFC,0x29,
    0x49,0xA0,0xBC,0xF8,0x48,0xE8,0xBF,0x70,0x11,0xA2,0x7C,0xE8,0x0A,0xA4,0xFC,0xE4,
    0x4A,0x92,0xBC,0xA0,0x4C,0x3C,0xFC,0x50,0x54,0xA6,0xFC,0xA0,0x53,0x3A,0xBC,0x54,
    0x53,0x56,0xBC,0x54,0x53,0x58,0xBC,0x54,0x04,0xA4,0xFC,0xA0,0x00,0xA8,0xFC,0xA0,
    0x4C,0x9E,0xBC,0xA0,0x4B,0xA0,0xBC,0xA1,0x00,0xA2,0xFC,0xA0,0x80,0xA2,0xFC,0x72,
    0xF2,0x8F,0x3C,0x61,0x21,0x9E,0xF8,0xE4,0x31,0x00,0x78,0x5C,0xF1,0xA1,0xBC,0x80,
    0x49,0xA0,0xBC,0xF8,0xF2,0x8F,0x3C,0x61,0x00,0xA3,0xFC,0x70,0x01,0xA2,0xFC,0x29,
    0x26,0x00,0x4C,0x5C,0x51,0xA8,0xBC,0x68,0x08,0xA8,0xFC,0x20,0x4D,0x3C,0xFC,0x50,
    0x1E,0xA4,0xFC,0xE4,0x01,0xA6,0xFC,0x80,0x19,0x00,0x7C,0x5C,0x1E,0x9E,0xBC,0xA0,
    0xFF,0x9F,0xFC,0x60,0x4C,0x9E,0x7C,0x86,0x00,0x84,0x68,0x0C,0x4E,0xA8,0x3C,0xC2,
    0x09,0x00,0x54,0x5C,0x01,0x9C,0xFC,0xC1,0x55,0x00,0x70,0x5C,0x55,0xA6,0xFC,0x84,
    0x40,0xAA,0x3C,0x08,0x04,0x80,0xFC,0x80,0x43,0x74,0xBC,0x80,0x3A,0xA6,0xFC,0xE4,
    0x55,0x74,0xFC,0x54,0x09,0x00,0x7C,0x5C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x80,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x80,0x00,0x00,0xFF,0xFF,0xF9,0xFF,
    0x10,0xC0,0x07,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x40,0xB6,0x02,0x00,0x00,
    0x5B,0x01,0x00,0x00,0x08,0x02,0x00,0x00,0x55,0x73,0xCB,0x00,0x50,0x45,0x01,0x00,
    0x00,0x00,0x00,0x00,0x35,0xC7,0x08,0x35,0x2C,0x32,0x00,0x00};
    
// Loader VerifyRAM snippet; use with ltVerifyRAM, ltProgramEEPROM.
static uint8_t verifyRAM[] = {
    0x44,0xA4,0xBC,0xA0,0x40,0xA4,0xBC,0x84,0x02,0xA4,0xFC,0x2A,0x40,0x82,0x14,0x08,
    0x04,0x80,0xD4,0x80,0x58,0xA4,0xD4,0xE4,0x0A,0xA4,0xFC,0x04,0x04,0xA4,0xFC,0x84,
    0x52,0x8A,0x3C,0x08,0x04,0xA4,0xFC,0x84,0x52,0x8A,0x3C,0x08,0x01,0x80,0xFC,0x84,
    0x40,0xA4,0xBC,0x00,0x52,0x82,0xBC,0x80,0x60,0x80,0x7C,0xE8,0x41,0x9C,0xBC,0xA4,
    0x09,0x00,0x7C,0x5C};

// Loader LaunchStart snippet; use with ltLaunchStart.
static uint8_t launchStart[] = {
    0xB8,0x68,0xFC,0x58,0x58,0x68,0xFC,0x50,0x09,0x00,0x7C,0x5C,0x06,0x80,0xFC,0x04,
    0x10,0x80,0x7C,0x86,0x00,0x84,0x54,0x0C,0x02,0x8C,0x7C,0x0C};

// Loader LaunchFinal snippet; use with ltLaunchFinal.
static uint8_t launchFinal[] = {
    0x06,0x80,0xFC,0x04,0x10,0x80,0x7C,0x86,0x00,0x84,0x54,0x0C,0x02,0x8C,0x7C,0x0C};

static uint8_t initCallFrame[] = {0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xF9, 0xFF};

static uint8_t *GenerateLoaderPacket(const uint8_t *image, int imageSize, int *pLength)
{
    int imageSizeInLongs = (imageSize + 3) / 4;
    uint8_t encodedImage[MAX_BUFFER_SIZE * 8]; // worst case assuming one byte per bit encoding
    int encodedImageSize, packetSize, tmp, i;
    uint8_t *packet, *p;
    
    /* encode the patched image */
    encodedImageSize = EncodeBytes(image, imageSize, encodedImage, sizeof(encodedImage));
    if (encodedImageSize < 0)
        return NULL;
    
    /* determine the size of the packet */
    packetSize = sizeof(txHandshake) + LENGTH_FIELD_SIZE + encodedImageSize;
    
    /* allocate space for the full packet */
    packet = (uint8_t *)malloc(packetSize);
    if (!packet)
        return NULL;
        
    /* copy the handshake image to the packet */
    memcpy(packet, txHandshake, sizeof(txHandshake));
    
    /* build the packet from the handshake data, the image length and the encoded image */
    p = packet + sizeof(txHandshake);
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

uint8_t *Loader::generateInitialLoaderPacket(int packetID, int *pLength)
{
    int initAreaOffset = sizeof(rawLoaderImage) + RAW_LOADER_INIT_OFFSET_FROM_END;
    uint8_t loaderImage[sizeof(rawLoaderImage)];
    int checksum, i;

    // Make a copy of the loader template
    memcpy(loaderImage, rawLoaderImage, sizeof(rawLoaderImage));
    
    // Initial Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  0, (int)trunc(80000000.0 / m_baudrate + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(80000000.0 / FINAL_BAUD + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(1.5 * 80000000.0 / FINAL_BAUD - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(2.0 * 80000000.0 / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc((2.0 * 80000000.0 / FINAL_BAUD) * (10.0 / 12.0) + 0.5));
    
    // First Expected Packet ID; total packet count.
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, packetID);

    // Recalculate and update checksum so low byte of checksum calculates to 0.
    checksum = 0;
    loaderImage[5] = 0; // start with a zero checksum
    for (i = 0; i < sizeof(rawLoaderImage); ++i)
        checksum += loaderImage[i];
    for (i = 0; i < sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];
    loaderImage[5] = 256 - (checksum & 0xFF);
    
    /* encode the image and form a packet */
    return GenerateLoaderPacket(loaderImage, sizeof(rawLoaderImage), pLength);
}

int Loader::transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult)
{
    int packetSize = 4 + payloadSize;
    uint8_t *packet, response[4];
    int result, cnt;
    
    /* build the packet to transmit */
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return -1;
    setLong(&packet[0], id);
    memcpy(&packet[4], payload, payloadSize);
    
    /* send the packet */
    printf("Sending %d, %d bytes -- ", id, packetSize); fflush(stdout);
    sendData(packet, packetSize);
    
    /* receive the response */
    cnt = receiveDataExact(response, sizeof(response), 2000);
    result = getLong(&response[0]);
    if (cnt != 4 || result == id) {
        printf("failed %d\n", result);
        free(packet);
        return -1;
    }
    printf("result %d\n", result);
    
    /* free the packet */
    free(packet);
    
    /* return successfully */
    *pResult = result;
    return 0;
}

static void msleep(int ms)
{
    usleep(ms * 1000);
}

void Loader::setBaudrate(int baudrate)
{
    m_baudrate = baudrate;
}

uint8_t *Loader::readEntireFile(const char *file, int *pLength)
{
    uint8_t *image;
    int imageSize;
    FILE *fp;
    
    /* open the file to load */
    if ((fp = fopen(file, "rb")) == NULL)
        return NULL;
    
    /* get the size of the binary file */
    fseek(fp, 0, SEEK_END);
    imageSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* allocate space for the file */
    if (!(image = (uint8_t *)malloc(imageSize))) {
        fclose(fp);
        return NULL;
    }
    
    /* read the entire image into memory */
    if (fread(image, 1, imageSize, fp) != imageSize) {
        free(image);
        fclose(fp);
        return NULL;
    }
    
    /* close the image file */
    fclose(fp);
    
    /* return the buffer containing the file contents */
    *pLength = imageSize;
    return image;
}

int Loader::loadFile(const char *file)
{
    int imageSize, sts;
    uint8_t *image;
    
    /* read the entire file into a buffer */
    if (!(image = readEntireFile(file, &imageSize)))
        return -1;
    
    /* load the image */
    sts = loadImage(image, imageSize);
    free(image);
    
    /* return load status */
    return sts;
}
    
int Loader::loadFile2(const char *file)
{
    int imageSize, sts;
    uint8_t *image;
    
    /* read the entire file into a buffer */
    if (!(image = readEntireFile(file, &imageSize)))
        return -1;
    
    /* load the image */
    sts = loadImage2(image, imageSize);
    free(image);
    
    /* return load status */
    return sts;
}
    
int Loader::loadImage(const uint8_t *image, int imageSize)
{
    int packetSize, sts;
    uint8_t *packet;

    /* generate a loader packet */
    packet = GenerateLoaderPacket(image, imageSize, &packetSize);
    if (!packet)
        return -1;

    /* connect to the target */
    if ((sts = connect()) != 0) {
        free(packet);
        return sts;
    }
    
    /* load the program using the propeller ROM protocol */
    if (loadSecondStageLoader(packet, packetSize) != 0)
        return -1;
        
    /* disconnect from the target */
    disconnect();
    
    /* return successfully */
    return 0;
}

int Loader::loadImage2(const uint8_t *image, int imageSize)
{
    uint8_t *packet, packet2[MAX_BUFFER_SIZE];
    int packetSize, cnt, sts, i;
    int32_t packetID, checksum;
    
    /* compute the packet ID (number of packets to be sent) */
    packetID = (imageSize + maxDataSize() - 1) / maxDataSize();

    /* generate a loader packet */
    packet = generateInitialLoaderPacket(packetID, &packetSize);
    if (!packet)
        return -1;
        
    /* compute the image checksum */
    checksum = 0;
    for (i = 0; i < imageSize; ++i)
        checksum += image[i];
    for (i = 0; i < sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];

    /* connect to the target */
    if ((sts = connect()) != 0) {
        free(packet);
        return sts;
    }
    
    /* load the second-stage loader using the propeller ROM protocol */
    if (loadSecondStageLoader(packet, packetSize) != 0)
        return -1;
            
    printf("Waiting for second-stage loader initial response\n");
    cnt = receiveData(packet2, sizeof(packet2));
    if (cnt != 4 || getLong(packet2) != packetID) {
        printf("error: second-stage loader failed to start\n");
        return 1;
    }
    printf("Got initial second-stage loader response\n");
    
    /* switch to the final baud rate */
    setBaudRate(FINAL_BAUD);
    
    /* transmit the image */
    int result;
    printf("Sending image: %d\n", packetID);
    while (imageSize > 0) {
        int size, sts;
        if ((size = imageSize) > maxDataSize())
            size = maxDataSize();
        sts = transmitPacket(packetID, image, size, &result);
        if (result != packetID - 1)
            printf("Unexpected result\n");
        imageSize -= size;
        image += size;
        --packetID;
    }
    
    /* transmit the RAM verify packet and verify the checksum */
    transmitPacket(0, verifyRAM, sizeof(verifyRAM), &result);
    if (result != -checksum)
        printf("Checksum error\n");
    
    /* transmit the final launch packets */
    transmitPacket(-checksum, launchStart, sizeof(launchStart), &result);
    if (result != -checksum - 1)
        printf("Launch failed\n");
    sendData(launchFinal, sizeof(launchFinal));
    
    /* disconnect from the target */
    disconnect();
    
    /* return successfully */
    return 0;
}

int Loader::loadSecondStageLoader(uint8_t *packet, int packetSize)
{
    uint8_t packet2[MAX_BUFFER_SIZE];
    int version, cnt, i;
    
    /* reset the Propeller */
    generateResetSignal();
    
    /* send the second-stage loader */
    printf("Send second-stage loader image\n");
    sendData(packet, packetSize);
    
    /* Reset period 200 ms + first packet’s serial transfer time + 20 ms */
    msleep(200 + (packetSize * 10 * 1000) / m_baudrate + 20);
    
    /* send the verification packet (all timing templates) */
    printf("Send verification packet\n");
    memset(packet2, 0xF9, sizeof(rxHandshake) + 4);
    sendData(packet2, sizeof(rxHandshake) + 4);
    
    /* this delay helps apply the majority of the next step’s receive timeout to a
       valid time window in the communication sequence */
    msleep((sizeof(packet2) * 10 * 1000) / m_baudrate);
    
    /* receive the handshake response and the hardware version */
    printf("Receive handshake response\n");
    cnt = receiveDataExact(packet2, sizeof(rxHandshake) + 4, 2000);
    
    /* verify the handshake response */
    if (cnt != sizeof(rxHandshake) + 4 || memcmp(packet2, rxHandshake, sizeof(rxHandshake)) != 0) {
        printf("error: handshake failed\n");
        return -1;
    }
    
    /* verify the hardware version */
    version = 0;
    for (i = sizeof(rxHandshake); i < cnt; ++i)
        version = ((version >> 2) & 0x3F) | ((packet2[i] & 0x01) << 6) | ((packet2[i] & 0x20) << 2);
    if (version != 1) {
        printf("error: wrong propeller version\n");
        return -1;
    }
    
    /* verify the checksum */
    printf("Receive checksum\n");
    cnt = receiveDataExact(packet2, 1, 2000);
    if (cnt != 1 || packet2[0] != 0xFE) {
        printf("error: loader checksum failed\n");
        return -1;
    }
    printf("Success!!\n");
    
    /* return successfully */
    return 0;
}
