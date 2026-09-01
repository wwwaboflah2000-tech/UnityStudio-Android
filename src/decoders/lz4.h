#ifndef LZ4_H
#define LZ4_H

#if defined (__cplusplus)
extern "C" {
#endif

int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize);

#if defined (__cplusplus)
}
#endif

#endif
