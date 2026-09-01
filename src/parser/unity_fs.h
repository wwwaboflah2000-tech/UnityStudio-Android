#ifndef UNITY_FS_H
#define UNITY_FS_H

#include "unity_reader.h"
#include <vector>
#include <string>

struct UnityFSHeader {
    std::string signature;      // "UnityFS"
    uint32_t version;
    std::string unity_version;
    std::string generator_version;
    uint64_t file_size;
    uint32_t compressed_blocks_size;
    uint32_t uncompressed_blocks_size;
    uint32_t flags;
};

struct StorageBlock {
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    uint16_t flags; // 0 = uncompressed, 1 = LZMA, 2/3 = LZ4/LZ4HC
};

struct NodeFile {
    uint64_t offset;
    uint64_t size;
    uint32_t flags;
    std::string path;
};

class UnityFSArchive {
public:
    UnityFSHeader header;
    std::vector<StorageBlock> blocks;
    std::vector<NodeFile> directory_nodes;
    std::vector<uint8_t> decompressed_data;

    bool parse(const uint8_t* raw_data, size_t raw_size);
};

#endif
