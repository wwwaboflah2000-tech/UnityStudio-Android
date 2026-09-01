#include "unity_fs.h"
#include <cstring>
#include <iostream>

// دالة فك ضغط LZ4 السريعة
extern "C" int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize);

bool UnityFSArchive::parse(const uint8_t* raw_data, size_t raw_size) {
    try {
        UnityReader reader(raw_data, raw_size);
        header.signature = reader.read_string_null();

        if (header.signature != "UnityFS") {
            return false;
        }

        header.version = reader.read_u32_be();
        header.unity_version = reader.read_string_null();
        header.generator_version = reader.read_string_null();
        header.file_size = reader.read_u64_be();
        header.compressed_blocks_size = reader.read_u32_be();
        header.uncompressed_blocks_size = reader.read_u32_be();
        header.flags = reader.read_u32_be();

        // قراءة كتلة البيانات الوصفية (Block Info)
        std::vector<uint8_t> block_info_data;
        if ((header.flags & 0x80) != 0) { // BlockInfo at end of file
            size_t saved_pos = reader.get_position();
            reader.set_position(raw_size - header.compressed_blocks_size);
            block_info_data = reader.read_bytes(header.compressed_blocks_size);
            reader.set_position(saved_pos);
        } else {
            block_info_data = reader.read_bytes(header.compressed_blocks_size);
        }

        // فك ضغط Block Info إذا كانت مضغوطة بـ LZ4
        std::vector<uint8_t> uncompressed_info(header.uncompressed_blocks_size);
        int compression_type = header.flags & 0x3F;
        
        if (compression_type == 2 || compression_type == 3) { // LZ4 / LZ4HC
            LZ4_decompress_safe(
                reinterpret_cast<const char*>(block_info_data.data()),
                reinterpret_cast<char*>(uncompressed_info.data()),
                header.compressed_blocks_size,
                header.uncompressed_blocks_size
            );
        } else {
            uncompressed_info = block_info_data;
        }

        // قراءة تفاصيل الـ Blocks والـ Directory Files من الكتلة المفكوكة
        UnityReader info_reader(uncompressed_info.data(), uncompressed_info.size());
        info_reader.set_position(16); // تخطي الـ uncompressed data GUID (16 bytes)

        uint32_t num_blocks = info_reader.read_u32_be();
        blocks.resize(num_blocks);
        size_t total_uncompressed_size = 0;

        for (uint32_t i = 0; i < num_blocks; ++i) {
            blocks[i].uncompressed_size = info_reader.read_u32_be();
            blocks[i].compressed_size = info_reader.read_u32_be();
            blocks[i].flags = (uint16_t)info_reader.read_u32_be(); // flags
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

        // فك ضغط كتل البيانات بالكامل داخل decompressed_data
        decompressed_data.resize(total_uncompressed_size);
        size_t write_offset = 0;

        for (const auto& b : blocks) {
            std::vector<uint8_t> comp_block = reader.read_bytes(b.compressed_size);
            int block_comp = b.flags & 0x3F;

            if (block_comp == 2 || block_comp == 3) { // LZ4
                LZ4_decompress_safe(
                    reinterpret_cast<const char*>(comp_block.data()),
                    reinterpret_cast<char*>(decompressed_data.data() + write_offset),
                    b.compressed_size,
                    b.uncompressed_size
                );
            } else {
                std::memcpy(decompressed_data.data() + write_offset, comp_block.data(), b.uncompressed_size);
            }
            write_offset += b.uncompressed_size;
        }

        return true;
    } catch (...) {
        return false;
    }
}
