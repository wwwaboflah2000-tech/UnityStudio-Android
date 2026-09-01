#include "serialized_file.h"

bool SerializedFile::parse(const uint8_t* raw_data, size_t raw_size, const std::string& name) {
    if (!raw_data || raw_size < 20) return false;

    file_name = name;
    UnityReader reader(raw_data, raw_size);

    // فحص ذكي لمعرفة هل الملف Unity حديث (64-bit v22+) أم قديم (32-bit v9..v21)
    reader.set_position(8);
    uint32_t v_old = reader.read_u32_be();
    reader.set_position(12);
    uint32_t v_new = reader.read_u32_be();

    if (v_new >= 22 && v_new < 100) {
        // Unity 2020.3+ / 2021 / 2022 / 2023 (64-bit Header)
        if (raw_size < 48) return false;
        reader.set_position(0);
        metadata_size = reader.read_u32_be();
        file_size = reader.read_u64_be();
        version = v_new;
        reader.set_position(16);
        data_offset = reader.read_u64_be();
        endianess = reader.read_u8();
        reader.align(4);
    } else if (v_old >= 1 && v_old < 22) {
        // Unity 5.x / 2017 / 2018 / 2019 (32-bit Header)
        reader.set_position(0);
        metadata_size = reader.read_u32_be();
        file_size = static_cast<uint64_t>(reader.read_u32_be());
        version = v_old;
        reader.set_position(12);
        data_offset = static_cast<uint64_t>(reader.read_u32_be());
        endianess = reader.read_u8();
        reader.align(4);
    } else {
        return false;
    }

    if (reader.failed()) return false;

    if (version >= 7) {
        unity_version = reader.read_string_null();
    }
    if (version >= 8) {
        reader.read_u32_le(); // Target Platform
    }

    // قراءة الـ TypeTree
    if (version >= 13) {
        bool enable_type_tree = reader.read_u8() != 0;
        int32_t type_count = reader.read_i32_le();

        if (type_count > 0 && type_count < 100000) {
            for (int32_t i = 0; i < type_count; ++i) {
                int32_t class_id = reader.read_i32_le();
                if (version >= 16) {
                    reader.read_u8();
                }
                int16_t script_type_index = -1;
                if (version >= 17) {
                    script_type_index = static_cast<int16_t>(reader.read_u16_le());
                }

                if ((version < 16 && class_id < 0) || (version >= 16 && class_id == 114) || (version >= 17 && script_type_index >= 0)) {
                    reader.read_bytes(16);
                }
                reader.read_bytes(16);

                if (enable_type_tree) {
                    int32_t node_count = reader.read_i32_le();
                    int32_t string_buffer_size = reader.read_i32_le();
                    size_t node_size = (version >= 19) ? 32 : 24;
                    reader.read_bytes(node_count * node_size + string_buffer_size);
                }
            }
        }
    }

    if (version >= 7 && version < 14) {
        reader.read_u32_le();
    }

    uint32_t object_count = (version >= 14) ? reader.read_u32_be() : reader.read_u32_le();
    
    if (object_count == 0 || object_count > 1000000 || reader.failed()) return false;
    
    objects.resize(object_count);

    for (uint32_t i = 0; i < object_count; ++i) {
        if (version >= 14) reader.align(4);

        objects[i].path_id = (version >= 14) ? static_cast<int64_t>(reader.read_u64_be()) : static_cast<int64_t>(reader.read_i32_be());
        objects[i].byte_start = (version >= 22) ? reader.read_u64_be() : reader.read_u32_be();
        objects[i].byte_start += data_offset;
        objects[i].byte_size = reader.read_u32_be();
        objects[i].type_id = reader.read_i32_le();
        objects[i].class_id = objects[i].type_id;
        objects[i].source_file = file_name;

        path_id_to_index[objects[i].path_id] = i;
    }

    return !reader.failed() && !objects.empty();
}
