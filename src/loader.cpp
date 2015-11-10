/* Propeller WiFi loader

  Based on Jeff Martin's Pascal loader and Mike Westerfield's iOS loader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.hpp"
#include "loadelf.h"

#define MAX_BUFFER_SIZE         4096        /* The maximum buffer size. (BUG: git rid of this magic number) */
#define FINAL_BAUD              921600      /* Final XBee-to-Propeller baud rate. */
#define MAX_RX_SENSE_ERROR      23          /* Maximum number of cycles by which the detection of a start bit could be off (as affected by the Loader code) */
#define LENGTH_FIELD_SIZE       11          /* number of bytes in the length field */

// Offset (in bytes) from end of Loader Image pointing to where most host-initialized values exist.
// Host-Initialized values are: Initial Bit Time, Final Bit Time, 1.5x Bit Time, Failsafe timeout,
// End of Packet timeout, and ExpectedID.  In addition, the image checksum at word 5 needs to be
// updated.  All these values need to be updated before the download stream is generated.
// NOTE: DAT block data is always placed before the first Spin method
#define RAW_LOADER_INIT_OFFSET_FROM_END (-(10 * 4) - 8)

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
//static uint8_t programShutdownCmd[] = {0xca, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM, Program EEPROM, and Run command (3); 11 bytes.
//static uint8_t programRunCmd[] = {0x25, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xfe};

//    // Download command (1; program RAM and run); 11 bytes.
//    0x93,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92,0xF2};

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

static uint8_t *LoadSpinBinaryFile(FILE *fp, int *pLength);
static uint8_t *LoadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize);

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

// Raw loader image.  This is a memory image of a Propeller Application written in PASM that fits into our initial
// download packet.  Once started, it assists with the remainder of the download (at a faster speed and with more
// relaxed interstitial timing conducive of Internet Protocol delivery. This memory image isn't used as-is; before
// download, it is first adjusted to contain special values assigned by this host (communication timing and
// synchronization values) and then is translated into an optimized Propeller Download Stream understandable by the
// Propeller ROM-based boot loader.
static uint8_t rawLoaderImage[] = {
    0x00,0xB4,0xC4,0x04,0x6F,0x2B,0x10,0x00,0x88,0x01,0x90,0x01,0x80,0x01,0x94,0x01,
    0x78,0x01,0x02,0x00,0x70,0x01,0x00,0x00,0x4D,0xE8,0xBF,0xA0,0x4D,0xEC,0xBF,0xA0,
    0x51,0xB8,0xBC,0xA1,0x01,0xB8,0xFC,0x28,0xF1,0xB9,0xBC,0x80,0xA0,0xB6,0xCC,0xA0,
    0x51,0xB8,0xBC,0xF8,0xF2,0x99,0x3C,0x61,0x05,0xB6,0xFC,0xE4,0x59,0x24,0xFC,0x54,
    0x62,0xB4,0xBC,0xA0,0x02,0xBC,0xFC,0xA0,0x51,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x04,0xBE,0xFC,0xA0,0x08,0xC0,0xFC,0xA0,0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x64,
    0x01,0xB2,0xFC,0x21,0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x70,0x12,0xC0,0xFC,0xE4,
    0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x68,0x0F,0xBE,0xFC,0xE4,0x48,0x24,0xBC,0x80,
    0x0E,0xBC,0xFC,0xE4,0x52,0xA2,0xBC,0xA0,0x54,0x44,0xFC,0x50,0x61,0xB4,0xFC,0xA0,
    0x5A,0x5E,0xBC,0x54,0x5A,0x60,0xBC,0x54,0x5A,0x62,0xBC,0x54,0x04,0xBE,0xFC,0xA0,
    0x54,0xB6,0xBC,0xA0,0x53,0xB8,0xBC,0xA1,0x00,0xBA,0xFC,0xA0,0x80,0xBA,0xFC,0x72,
    0xF2,0x99,0x3C,0x61,0x25,0xB6,0xF8,0xE4,0x36,0x00,0x78,0x5C,0xF1,0xB9,0xBC,0x80,
    0x51,0xB8,0xBC,0xF8,0xF2,0x99,0x3C,0x61,0x00,0xBB,0xFC,0x70,0x01,0xBA,0xFC,0x29,
    0x2A,0x00,0x4C,0x5C,0xFF,0xC2,0xFC,0x64,0x5D,0xC2,0xBC,0x68,0x08,0xC2,0xFC,0x20,
    0x55,0x44,0xFC,0x50,0x22,0xBE,0xFC,0xE4,0x01,0xB4,0xFC,0x80,0x1E,0x00,0x7C,0x5C,
    0x22,0xB6,0xBC,0xA0,0xFF,0xB7,0xFC,0x60,0x54,0xB6,0x7C,0x86,0x00,0x8E,0x68,0x0C,
    0x59,0xC2,0x3C,0xC2,0x09,0x00,0x54,0x5C,0x01,0xB2,0xFC,0xC1,0x63,0x00,0x70,0x5C,
    0x63,0xB4,0xFC,0x84,0x45,0xC6,0x3C,0x08,0x04,0x8A,0xFC,0x80,0x48,0x7E,0xBC,0x80,
    0x3F,0xB4,0xFC,0xE4,0x63,0x7E,0xFC,0x54,0x09,0x00,0x7C,0x5C,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x80,0x00,0x00,
    0xFF,0xFF,0xF9,0xFF,0x10,0xC0,0x07,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x40,
    0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x10,0x6F,0x00,0x00,0x00,0xB6,0x02,0x00,0x00,
    0x56,0x00,0x00,0x00,0x82,0x00,0x00,0x00,0x55,0x73,0xCB,0x00,0x18,0x51,0x00,0x00,
    0x30,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x68,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x35,0xC7,0x08,0x35,0x2C,0x32,0x00,0x00};
    
// Loader VerifyRAM snippet.
static uint8_t verifyRAM[] = {
    0x49,0xBC,0xBC,0xA0,0x45,0xBC,0xBC,0x84,0x02,0xBC,0xFC,0x2A,0x45,0x8C,0x14,0x08,
    0x04,0x8A,0xD4,0x80,0x66,0xBC,0xD4,0xE4,0x0A,0xBC,0xFC,0x04,0x04,0xBC,0xFC,0x84,
    0x5E,0x94,0x3C,0x08,0x04,0xBC,0xFC,0x84,0x5E,0x94,0x3C,0x08,0x01,0x8A,0xFC,0x84,
    0x45,0xBE,0xBC,0x00,0x5F,0x8C,0xBC,0x80,0x6E,0x8A,0x7C,0xE8,0x46,0xB2,0xBC,0xA4,
    0x09,0x00,0x7C,0x5C};
    
// Loader ProgramVerifyEEPROM snippet.
static uint8_t programVerifyEEPROM[] = {
    0x03,0x8C,0xFC,0x2C,0x4F,0xEC,0xBF,0x68,0x82,0x18,0xFD,0x5C,0x40,0xBE,0xFC,0xA0,
    0x45,0xBA,0xBC,0x00,0xA0,0x62,0xFD,0x5C,0x79,0x00,0x70,0x5C,0x01,0x8A,0xFC,0x80,
    0x67,0xBE,0xFC,0xE4,0x8F,0x3E,0xFD,0x5C,0x49,0x8A,0x3C,0x86,0x65,0x00,0x54,0x5C,
    0x00,0x8A,0xFC,0xA0,0x49,0xBE,0xBC,0xA0,0x7D,0x02,0xFD,0x5C,0xA3,0x62,0xFD,0x5C,
    0x45,0xC0,0xBC,0x00,0x5D,0xC0,0x3C,0x86,0x79,0x00,0x54,0x5C,0x01,0x8A,0xFC,0x80,
    0x72,0xBE,0xFC,0xE4,0x01,0x8C,0xFC,0x28,0x8F,0x3E,0xFD,0x5C,0x01,0x8C,0xFC,0x28,
    0x46,0xB2,0xBC,0xA4,0x09,0x00,0x7C,0x5C,0x82,0x18,0xFD,0x5C,0xA1,0xBA,0xFC,0xA0,
    0x8D,0x62,0xFD,0x5C,0x79,0x00,0x70,0x5C,0x00,0x00,0x7C,0x5C,0xFF,0xBD,0xFC,0xA0,
    0xA0,0xBA,0xFC,0xA0,0x8D,0x62,0xFD,0x5C,0x83,0xBC,0xF0,0xE4,0x45,0xBA,0x8C,0xA0,
    0x08,0xBA,0xCC,0x28,0xA0,0x62,0xCD,0x5C,0x45,0xBA,0x8C,0xA0,0xA0,0x62,0xCD,0x5C,
    0x79,0x00,0x70,0x5C,0x00,0x00,0x7C,0x5C,0x47,0x8E,0x3C,0x62,0x90,0x00,0x7C,0x5C,
    0x47,0x8E,0x3C,0x66,0x09,0xC0,0xFC,0xA0,0x58,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x4F,0xE8,0xBF,0x64,0x4E,0xEC,0xBF,0x78,0x56,0xB8,0xBC,0xF8,0x4F,0xE8,0xBF,0x68,
    0xF2,0x9D,0x3C,0x61,0x56,0xB8,0xBC,0xF8,0x4E,0xEC,0xBB,0x7C,0x00,0xB8,0xF8,0xF8,
    0xF2,0x9D,0x28,0x61,0x91,0xC0,0xCC,0xE4,0x79,0x00,0x44,0x5C,0x7B,0x00,0x48,0x5C,
    0x00,0x00,0x68,0x5C,0x01,0xBA,0xFC,0x2C,0x01,0xBA,0xFC,0x68,0xA4,0x00,0x7C,0x5C,
    0xFE,0xBB,0xFC,0xA0,0x09,0xC0,0xFC,0xA0,0x58,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x4F,0xE8,0xBF,0x64,0x00,0xBB,0x7C,0x62,0x01,0xBA,0xFC,0x34,0x4E,0xEC,0xBF,0x78,
    0x57,0xB8,0xBC,0xF8,0x4F,0xE8,0xBF,0x68,0xF2,0x9D,0x3C,0x61,0x58,0xB8,0xBC,0xF8,
    0xA7,0xC0,0xFC,0xE4,0xFF,0xBA,0xFC,0x60,0x00,0x00,0x7C,0x5C};

// Loader readyToLaunch snippet.
static uint8_t readyToLaunch[] = {
    0xB8,0x72,0xFC,0x58,0x66,0x72,0xFC,0x50,0x09,0x00,0x7C,0x5C,0x06,0xBE,0xFC,0x04,
    0x10,0xBE,0x7C,0x86,0x00,0x8E,0x54,0x0C,0x04,0xBE,0xFC,0x00,0x78,0xBE,0xFC,0x60,
    0x50,0xBE,0xBC,0x68,0x00,0xBE,0x7C,0x0C,0x40,0xAE,0xFC,0x2C,0x6E,0xAE,0xFC,0xE4,
    0x04,0xBE,0xFC,0x00,0x00,0xBE,0x7C,0x0C,0x02,0x96,0x7C,0x0C};

// Loader LaunchNow snippet.
static uint8_t launchNow[] = {
    0x66,0x00,0x7C,0x5C};

static uint8_t initCallFrame[] = {0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xF9, 0xFF};

static uint8_t *GenerateIdentifyPacket(int *pLength)
{
    uint8_t *packet;
    int packetSize;
    
    /* determine the size of the packet */
    packetSize = sizeof(txHandshake) + sizeof(shutdownCmd);
    
    /* allocate space for the full packet */
    packet = (uint8_t *)malloc(packetSize);
    if (!packet)
        return NULL;
        
    /* copy the handshake image and the command to the packet */
    memcpy(packet, txHandshake, sizeof(txHandshake));
    memcpy(packet + sizeof(txHandshake), shutdownCmd, sizeof(shutdownCmd));
        
    /* return the packet and its length */
    *pLength = packetSize;
    return packet;
}

static uint8_t *GenerateLoaderPacket(const uint8_t *image, int imageSize, int *pLength)
{
    int imageSizeInLongs = (imageSize + 3) / 4;
    uint8_t encodedImage[MAX_BUFFER_SIZE * 8]; // worst case assuming one byte per bit encoding
    int encodedImageSize, packetSize, tmp, i;
    uint8_t *packet, *p;
    
    /* encode the image */
    encodedImageSize = EncodeBytes(image, imageSize, encodedImage, sizeof(encodedImage));
    if (encodedImageSize < 0)
        return NULL;
    
    /* determine the size of the packet */
    packetSize = sizeof(txHandshake) + sizeof(loadRunCmd) + LENGTH_FIELD_SIZE + encodedImageSize;
    
    /* allocate space for the full packet */
    packet = (uint8_t *)malloc(packetSize);
    if (!packet)
        return NULL;
        
    /* copy the handshake image and the command to the packet */
    memcpy(packet, txHandshake, sizeof(txHandshake));
    memcpy(packet + sizeof(txHandshake), loadRunCmd, sizeof(loadRunCmd));
    
    /* build the packet from the handshake data, the image length and the encoded image */
    p = packet + sizeof(txHandshake) + sizeof(loadRunCmd);
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

double ClockSpeed = 80000000.0;
int FinalBaud = FINAL_BAUD;

uint8_t *Loader::generateInitialLoaderPacket(int packetID, int *pLength)
{
    int initAreaOffset = sizeof(rawLoaderImage) + RAW_LOADER_INIT_OFFSET_FROM_END;
    uint8_t loaderImage[sizeof(rawLoaderImage)];
    int checksum, i;

    // Make a copy of the loader template
    memcpy(loaderImage, rawLoaderImage, sizeof(rawLoaderImage));
    
    // Clock mode
    //SetHostInitializedValue(loaderImage, initAreaOffset +  0, 0);

    // Initial Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(80000000.0 / m_baudrate + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(80000000.0 / FinalBaud + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(1.5 * ClockSpeed / FinalBaud - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc(2.0 * ClockSpeed / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, (int)trunc((2.0 * ClockSpeed / FinalBaud) * (10.0 / 12.0) + 0.5));
    
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
    
    /* encode the image and form a packet */
    return GenerateLoaderPacket(loaderImage, sizeof(rawLoaderImage), pLength);
}

void Loader::setBaudrate(int baudrate)
{
    m_baudrate = baudrate;
}

int Loader::identify(int *pVersion)
{
    uint8_t packet2[MAX_BUFFER_SIZE]; // must be at least as big as maxDataSize()
    int version, cnt, i;
    uint8_t *packet;
    int packetSize;
    
    /* generate the identify packet */
    if (!(packet = GenerateIdentifyPacket(&packetSize))) {
        printf("error: generating identify packet\n");
        goto fail;
    }

    /* reset the Propeller */
    generateResetSignal();
    
    /* send the identify packet */
    sendData(packet, packetSize);
    
    pauseForVerification(packetSize);
    
    /* send the verification packet (all timing templates) */
    memset(packet2, 0xF9, maxDataSize());
    sendData(packet2, maxDataSize());
    
    pauseForChecksum(sizeof(packet2));
    
    /* receive the handshake response and the hardware version */
    cnt = receiveDataExact(packet2, sizeof(rxHandshake) + 4, 2000);
    if (cnt < 0)
        goto fail;
    
    /* verify the handshake response */
    if (cnt != sizeof(rxHandshake) + 4 || memcmp(packet2, rxHandshake, sizeof(rxHandshake)) != 0) {
        printf("error: handshake failed\n");
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

int Loader::loadFile(const char *file, LoadType loadType)
{
    int imageSize, sts;
    uint8_t *image;
    ElfHdr elfHdr;
    FILE *fp;

    /* open the binary */
    if (!(fp = fopen(file, "rb"))) {
        printf("error: can't open '%s'\n", file);
        return -1;
    }
    
    /* check for an elf file */
    if (ReadAndCheckElfHdr(fp, &elfHdr)) {
        image = LoadElfFile(fp, &elfHdr, &imageSize);
        fclose(fp);
    }
    else {
        image = LoadSpinBinaryFile(fp, &imageSize);
        fclose(fp);
    }
    
    /* make sure the image was loaded into memory */
    if (!image)
        return -1;
    
    /* load the file */
    if ((sts = loadImage(image, imageSize, loadType)) != 0) {
        free(image);
        return -1;
    }
    
    /* return successfully */
    free(image);
    return 0;
}

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

static uint8_t *LoadSpinBinaryFile(FILE *fp, int *pLength)
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
    *pLength = imageSize;
    return image;
}

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

static uint8_t *LoadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize)
{
    uint32_t start, imageSize, cogImagesSize;
    uint8_t *image, *buf, *p;
    ElfProgramHdr program;
    int chksum, cnt, i;
    SpinHdr *spinHdr;
    ElfContext *c;

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
    spinHdr->chksum = chksum = 0;
    p = image;
    for (cnt = imageSize; --cnt >= 0; )
        chksum += *p++;
    spinHdr->chksum = SPIN_TARGET_CHECKSUM - chksum;

    /* return the image */
    *pImageSize = imageSize;
    return image;
    
fail:
    /* return failure */
    FreeElfContext(c);
    return NULL;
}

int Loader::loadTinyImage(const uint8_t *image, int imageSize)
{
    uint8_t *packet;
    int packetSize;

    /* generate a loader packet */
    packet = GenerateLoaderPacket(image, imageSize, &packetSize);
    if (!packet)
        return -1;

    /* load the program using the propeller ROM protocol */
    if (loadSecondStageLoader(packet, packetSize) != 0)
        return -1;
        
    /* return successfully */
    return 0;
}

int Loader::loadImage(const uint8_t *image, int imageSize, LoadType loadType)
{
    uint8_t *packet, response[8];
    int packetSize, result, cnt, i;
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
    for (i = 0; i < (int)sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];

    /* load the second-stage loader using the propeller ROM protocol */
    if (loadSecondStageLoader(packet, packetSize) != 0)
        return -1;
            
    //printf("Waiting for second-stage loader initial response\n");
    cnt = receiveDataExact(response, sizeof(response), 2000);
    result = getLong(&response[0]);
    if (cnt != 8 || result != packetID) {
        printf("error: second-stage loader failed to start - cnt %d, packetID %d, result %d\n", cnt, packetID, result);
        return -1;
    }
    //printf("Got initial second-stage loader response\n");
    
    /* switch to the final baud rate */
    setBaudRate(FINAL_BAUD);
    
    /* transmit the image */
    while (imageSize > 0) {
        int size;
        if ((size = imageSize) > maxDataSize())
            size = maxDataSize();
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
    transmitPacket(packetID, verifyRAM, sizeof(verifyRAM), &result);
    if (result != -checksum)
        printf("Checksum error\n");
    packetID = -checksum;
    
    if (loadType & ltDownloadAndProgramEeprom) {
        //printf("Programming EEPROM\n");
        transmitPacket(packetID, programVerifyEEPROM, sizeof(programVerifyEEPROM), &result, 8000);
        if (result != -checksum*2)
            printf("Checksum error: expected %08x, got %08x\n", -checksum, result);
        packetID = -checksum*2;
    }
    
    /* transmit the final launch packets */
    
    //printf("Sending readyToLaunch packet\n");
    transmitPacket(packetID, readyToLaunch, sizeof(readyToLaunch), &result);
    if (result != packetID - 1)
        printf("ReadyToLaunch failed\n");
    --packetID;
    
    //printf("Sending launchNow packet\n");
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
        sendData(packet, packetSize);
    
        /* receive the response */
        if (pResult) {
            cnt = receiveDataExact(response, sizeof(response), timeout);
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

int Loader::loadSecondStageLoader(uint8_t *packet, int packetSize)
{
    uint8_t packet2[MAX_BUFFER_SIZE]; // must be at least as big as maxDataSize()
    int version, cnt, i;
    
    /* reset the Propeller */
    generateResetSignal();
    
    /* send the second-stage loader */
    //printf("Send second-stage loader image\n");
    sendData(packet, packetSize);
    
    pauseForVerification(packetSize);
    
    /* send the verification packet (all timing templates) */
    //printf("Send verification packet\n");
    memset(packet2, 0xF9, maxDataSize());
    sendData(packet2, maxDataSize());
    
    pauseForChecksum(sizeof(packet2));
    
    /* receive the handshake response and the hardware version */
    //printf("Receive handshake response\n");
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
    //printf("Receive checksum\n");
    cnt = receiveDataExact(packet2, 1, 2000);
    if (cnt != 1 || packet2[0] != 0xFE) {
        printf("error: loader checksum failed\n");
        return -1;
    }
    
    /* return successfully */
    return 0;
}
