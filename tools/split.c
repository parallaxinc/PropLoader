#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//#define DEBUG

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

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
    uint16_t xxxxx; // jeff: what is this?
    uint16_t yyyyy; // jeff: what is this?
    uint16_t zzzzz; // jeff: what is this?
} SpinHdr;

#define PACKET_CODE 0x11111111

static char *overlayNames[] = {
    "verifyRAM",
    "programVerifyEEPROM",
    "readyToLaunch",
    "launchNow"
};
static int overlayNameCount = sizeof(overlayNames) / sizeof(char *);

#ifdef DEBUG
static void DumpSpinHdr(FILE *fp, const char *tag, SpinHdr *hdr);
#endif
static void DumpRange(FILE *fp, uint8_t *image, int imageOffset, int endOffset);

int main(int argc, char *argv[])
{
    uint8_t *image;
    int imageSize, imageOffset, delta, oldSpinCodeOffset, firstOverlayMarker, overlayIndex, overlayOffset, chksum;
    FILE *ifp, *ofp;
    uint32_t *imagePtr, value;
    SpinHdr *hdr;
    
    if (argc != 3) {
        printf("usage: split <infile> <outfile>\n");
        return 1;
    }
    
    /* open the image file */
    if (!(ifp = fopen(argv[1], "rb"))) {
        printf("error: can't open '%s'\n", argv[1]);
        return 1;
    }
    
    /* determine the file size */
    fseek(ifp, 0, SEEK_END);
    imageSize = (int)ftell(ifp);
    fseek(ifp, 0, SEEK_SET);
    
    /* allocate space for the image */
    if (!(image = (uint8_t *)malloc(imageSize))) {
        printf("error: insufficient memory\n");
        return 1;
    }
    hdr = (SpinHdr *)image;
    
    /* read the entire image into memory */
    if (fread(image, 1, imageSize, ifp) != imageSize) {
        printf("error: reading image\n");
        return 1;
    }
    fclose(ifp);
    
    if (!(ofp = fopen(argv[2], "w"))) {
        printf("error: can't create '%s'\n", argv[2]);
        return 1;
    }
    
    /* dump the original image header */
#ifdef DEBUG
    DumpSpinHdr(ofp, "original", hdr);
#endif
    
    oldSpinCodeOffset = hdr->pcurr;
    
    firstOverlayMarker = -1;
    imagePtr = (uint32_t *)image;
    imageOffset = 0;
    while (imageOffset < imageSize) {
        if (*imagePtr++ == PACKET_CODE) {
            firstOverlayMarker = imageOffset;
            break;
        }
        imageOffset += sizeof(uint32_t);
    }
    
    if (firstOverlayMarker == -1) {
        printf("error: no overlays found\n");
        return 1;
    }
#ifdef DEBUG
    fprintf(ofp, "/* firstOverlayMarker: %04x */\n\n", firstOverlayMarker);
#endif

    /* fixup the spin header to remove the overlays */
    delta = hdr->pcurr - firstOverlayMarker;
    hdr->vbase -= delta;
    hdr->dbase -= delta;
    hdr->pcurr -= delta;
    hdr->dcurr -= delta;
    hdr->xxxxx -= delta;
    hdr->zzzzz -= delta;

    /* recompute the image checksum */
    hdr->chksum = chksum = 0;
    for (imageOffset = 0; imageOffset < firstOverlayMarker; ++imageOffset)
        chksum += image[imageOffset];
    for (imageOffset = oldSpinCodeOffset; imageOffset < imageSize; ++imageOffset)
        chksum += image[imageOffset];
    hdr->chksum = SPIN_TARGET_CHECKSUM - chksum;

    /* dump the patched spin header */
#ifdef DEBUG
    DumpSpinHdr(ofp, "patched", hdr);
#endif
    
    fprintf(ofp, "static uint8_t rawLoaderImage[] = {");
    DumpRange(ofp, image, 0, firstOverlayMarker); putc(',', ofp);
    DumpRange(ofp, image, oldSpinCodeOffset, imageSize);
    fprintf(ofp, "};\n\n");
    
    imageOffset = firstOverlayMarker + sizeof(uint32_t);
    imagePtr = (uint32_t *)(image + imageOffset);
    overlayOffset = imageOffset;
    overlayIndex = 0;
    while (imageOffset < oldSpinCodeOffset) {
        value = *imagePtr++;
        if (value == PACKET_CODE) {
            char *overlayName = (overlayIndex < overlayNameCount ? overlayNames[overlayIndex++] : "unknown");
            fprintf(ofp, "static uint8_t %s[] = {", overlayName);
            DumpRange(ofp, image, overlayOffset, imageOffset);
            fprintf(ofp, "};\n\n");
            overlayOffset = imageOffset + sizeof(uint32_t);
        }
        imageOffset += sizeof(uint32_t);
    }
    fprintf(ofp, "static uint8_t %s[] = {", overlayIndex < overlayNameCount ? overlayNames[overlayIndex++] : "unknown");
    DumpRange(ofp, image, overlayOffset, imageOffset);
    fprintf(ofp, "};\n");
    
    fclose(ofp);
    
    return 0;
}

#ifdef DEBUG
static void DumpSpinHdr(FILE *fp, const char *tag, SpinHdr *hdr)
{
    fprintf(fp, "/* %s: \n", tag);
    fprintf(fp, "    clkfreq: %d\n", hdr->clkfreq);
    fprintf(fp, "    clkmode: %02x\n", hdr->clkmode);
    fprintf(fp, "    chksum:  %02x\n", hdr->chksum);
    fprintf(fp, "    pbase:   %04x\n", hdr->pbase);
    fprintf(fp, "    vbase:   %04x\n", hdr->vbase);
    fprintf(fp, "    dbase:   %04x\n", hdr->dbase);
    fprintf(fp, "    pcurr:   %04x\n", hdr->pcurr);
    fprintf(fp, "    dcurr:   %04x\n", hdr->dcurr);
    fprintf(fp, "    xxxxx:   %04x\n", hdr->xxxxx);
    fprintf(fp, "    yyyyy:   %04x\n", hdr->yyyyy);
    fprintf(fp, "    zzzzz:   %04x\n", hdr->zzzzz);
    fprintf(fp, "*/\n\n");
}
#endif

static void DumpRange(FILE *fp, uint8_t *image, int imageOffset, int endOffset)
{
    uint32_t *imagePtr = (uint32_t *)(image + imageOffset);
    int needComma = 0;
    int cnt = 0;
    while (imageOffset < endOffset) {
        uint32_t value = *imagePtr++;
        if (needComma) {
            fprintf(fp, ",");
            needComma = 0;
        }
        if ((cnt % 16) == 0)
            fprintf(fp, "\n/* %04x */ ", imageOffset);
        fprintf(fp, "0x%02X,0x%02X,0x%02X,0x%02X",
                ( value        & 0xff),
                ((value >>  8) & 0xff),
                ((value >> 16) & 0xff),
                ((value >> 24) & 0xff));
        imageOffset += sizeof(uint32_t);
        needComma = 1;
        cnt += 4;
    }
}
