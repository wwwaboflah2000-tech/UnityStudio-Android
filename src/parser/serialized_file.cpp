#include "serialized_file.h"

bool SerializedFile::parse(const uint8_t* raw_data, size_t raw_size) {
    if (!raw_data || raw_size < 20) return false;

    UnityReader reader(raw_data, raw_size);

    metadata_size = reader.read_u32_be();
    file_size = reader.read_u32_be();
    version = reader.read_u32_be();
    data_offset = reader.read_u32_be();

    if (version >= 22) {
        if (raw_size < 48) return false;
        reader.set_position(0);
        metadata_size = reader.read_u32_be();
        file_size = static_cast<uint64_t>(reader.read_u64_be());
        version = reader.read_u32_be();
        data_offset = reader.read_u64_be();
        endianess = reader.read_u8();
        reader.align(4);
    }

    if (reader.failed()) return false;

    // قراءة جدول الكائنات (Object Table)
    if (version >= 7) {
        unity_version = reader.read_string_null();
    }
    if (version >= 8) {
        reader.read_u32_le(); // Target Platform
    }
    if (version >= 13) {
        bool enable_type_tree = reader.read_u8() != 0;
        // تخطي TypeTree للسرعة في هذه المرحلة
    }

    uint32_t object_count = (version >= 14) ? reader.read_u32_be() : reader.read_u32_le();
    objects.resize(object_count);

    for (uint32_t i = 0; i < object_count; ++i) {
        if (version >= 14) reader.align(4);

        objects[i].path_id = (version >= 14) ? static_cast<int64_t>(reader.read_u64_be()) : static_cast<int64_t>(reader.read_i32_be());
        objects[i].byte_start = (version >= 22) ? reader.read_u64_be() : reader.read_u32_be();
        objects[i].byte_start += data_offset;
        objects[i].byte_size = reader.read_u32_be();
        objects[i].type_id = reader.read_i32_be();
        objects[i].class_id = objects[i].type_id;

        path_id_to_index[objects[i].path_id] = i;
    }

    return !reader.failed();
}
