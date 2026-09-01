#include "unity_fs.h"
#include <cstring>

extern "C" int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize);

bool UnityFSArchive::parse(const uint8_t* raw_data, size_t raw_size) {
    if (!raw_data || raw_size < 16) return false;

    UnityReader reader(raw_data, raw_size);
    header.signature = reader.read_string_null();

    if (header.signature != "UnityFS" || reader.failed()) {
        return false;
    }

    header.version = reader.read_u32_be();
    header.unity_version = reader.read_string_null();
    header.generator_version = reader.read_string_null();
    header.file_size = reader.read_u64_be();
    header.compressed_blocks_size = reader.read_u32_be();
    header.uncompressed_blocks_size = reader.read_u32_be();
    header.flags = reader.read_u32_be();

    if (reader.failed() || header.compressed_blocks_size == 0) {
        return false;
    }

    // قراءة كتلة البيانات الوصفية (Block Info)
    std::vector<uint8_t> block_info_data;
    if ((header.flags & 0x80) != 0) { // BlockInfo at end of file
        size_t saved_pos = reader.get_position();
        if (raw_size < header.compressed_blocks_size) return false;
        reader.set_position(raw_size - header.compressed_blocks_size);
        block_info_data = reader.read_bytes(header.compressed_blocks_size);
        reader.set_position(saved_pos);
    } else {
        block_info_data = reader.read_bytes(header.compressed_blocks_size);
    }

    if (block_info_data.empty()) return false;

    // فك ضغط Block Info
    std::vector<uint8_t> uncompressed_info(header.uncompressed_blocks_size);
    int compression_type = header.flags & 0x3F;
    
    if (compression_type == 2 || compression_type == 3) { // LZ4 / LZ4HC
        int decompressed = LZ4_decompress_safe(
            reinterpret_cast<const char*>(block_info_data.data()),
            reinterpret_cast<char*>(uncompressed_info.data()),
            header.compressed_blocks_size,
            header.uncompressed_blocks_size
        );
        if (decompressed < 0) return false;
    } else {
        uncompressed_info = block_info_data;
    }

    // قراءة تفاصيل الـ Blocks والـ Directory Files
    UnityReader info_reader(uncompressed_info.data(), uncompressed_info.size());
    info_reader.set_position(16); // تخطي الـ GUID

    uint32_t num_blocks = info_reader.read_u32_be();
    blocks.resize(num_blocks);
    size_t total_uncompressed_size = 0;

    for (uint32_t i = 0; i < num_blocks; ++i) {
        blocks[i].uncompressed_size = info_reader.read_u32_be();
        blocks[i].compressed_size = info_reader.read_u32_be();
        blocks[i].flags = static_cast<uint16_t>(info_reader.read_u32_be());
        total_uncompressed_size += blocks[i].uncompressed_size;
    }

    uint32_t num_nodes = info_reader.read_u32_be();
    directory_nodes.resize(num_nodes);
    for (uint32_t i = 0; i < num_nodes; ++i) {
        directory_nodes[i].offset = info_reader.read_u64_be();
        directory_nodes[i].size = info_reader.read_u64_be();
        directory_nodes[i].flags = info_reader.read_u32_be();
        directory_nodes[i].path = info_reader.read_string_null();
    }

    if (info_reader.failed()) return false;

    // فك ضغط كتل البيانات بالكامل
    decompressed_data.resize(total_uncompressed_size);
    size_t write_offset = 0;

    for (const auto& b : blocks) {
        std::vector<uint8_t> comp_block = reader.read_bytes(b.compressed_size);
        if (comp_block.empty()) return false;

        int block_comp = b.flags & 0x3F;
        if (block_comp == 2 || block_comp == 3) { // LZ4
            int res = LZ4_decompress_safe(
                reinterpret_cast<const char*>(comp_block.data()),
                reinterpret_cast<char*>(decompressed_data.data() + write_offset),
                b.compressed_size,
                b.uncompressed_size
            );
            if (res < 0) return false;
        } else {
            std::memcpy(decompressed_data.data() + write_offset, comp_block.data(), b.uncompressed_size);
        }
        write_offset += b.uncompressed_size;
    }

    return !reader.failed();
}
