#include "lz4.h"
#include <string.h>

int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize) {
    const unsigned char* ip = (const unsigned char*)source;
    const unsigned char* const iend = ip + compressedSize;
    unsigned char* op = (unsigned char*)dest;
    unsigned char* const oend = op + maxDecompressedSize;

    while (ip < iend) {
        unsigned token = *ip++;
        size_t length = token >> 4;

        if (length == 15) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while (ip < iend && s == 255);
        }

        if (op + length > oend || ip + length > iend) return -1;
        memcpy(op, ip, length);
        ip += length;
        op += length;

        if (ip >= iend) break;

        unsigned offset = ip[0] | (ip[1] << 8);
        ip += 2;
        if (offset == 0) return -1;

        length = token & 0x0F;
        if (length == 15) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while (ip < iend && s == 255);
        }
        length += 4;

        const unsigned char* match = op - offset;
        if (match < (const unsigned char*)dest || op + length > oend) return -1;

        while (length--) {
            *op++ = *match++;
        }
    }
    return (int)(op - (unsigned char*)dest);
}
