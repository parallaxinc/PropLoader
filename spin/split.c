#include <stdio.h>
#include <stdint.h>

#define PACKET_CODE 0x11111111

int main(int argc, char *argv[])
{
    int cnt, needComma;
    FILE *ifp, *ofp;
    uint32_t value;
    
    if (argc != 3) {
        printf("usage: split <infile> <outfile>\n");
        return 1;
    }
    
    if (!(ifp = fopen(argv[1], "rb"))) {
        printf("error: can't open '%s'\n", argv[1]);
        return 1;
    }
    
    if (!(ofp = fopen(argv[2], "w"))) {
        printf("error: can't create '%s'\n", argv[2]);
        return 1;
    }
    
    fprintf(ofp, "static uint8_t rawLoaderImage[] = {");
    cnt = 0;
    
    while (fread(&value, 1, sizeof(value), ifp) == sizeof(value)) {
        if (value == PACKET_CODE) {
            fprintf(ofp, "};\n\n");
            fprintf(ofp, "static uint8_t verifyRAM[] = {");
            cnt = 0;
        }
        else {
            if (needComma) {
                fprintf(ofp, ",");
                needComma = 0;
            }
            if ((cnt % 16) == 0)
                fprintf(ofp, "\n    ");
            fprintf(ofp, "%02X,%02X,%02X,%02X",
                    ( value        & 0xff),
                    ((value >>  8) & 0xff),
                    ((value >> 16) & 0xff),
                    ((value >> 24) & 0xff));
            needComma = 1;
            cnt += 4;
        }
    }
    fprintf(ofp, "};\n");
    
    fclose(ifp);
    fclose(ofp);
    
    return 0;
}
