#include "serialized_file.h"

bool SerializedFile::parse(const uint8_t* raw_data, size_t raw_size, const std::string& name) {
    if (!raw_data || raw_size < 20) return false;

    file_name = name;
    UnityReader reader(raw_data, raw_size);

    // 1. قراءة الترويسة الأساسية (Big Endian)
    metadata_size = reader.read_u32_be();
    file_size = static_cast<uint64_t>(reader.read_u32_be());
    version = reader.read_u32_be();
    data_offset = static_cast<uint64_t>(reader.read_u32_be());

    if (version >= 9) {
        endianess = reader.read_u8();
        reader.read_bytes(3); // m_Reserved
    } else {
        if (file_size >= metadata_size) {
            reader.set_position(file_size - metadata_size);
        }
        endianess = 0;
    }

    // مطابقة AssetStudio لإصدار Unity 22 (The Long Drive)
    if (version >= 22) {
        if (raw_size < 48) return false;
        metadata_size = reader.read_u32_be();
        file_size = reader.read_u64_be();
        data_offset = reader.read_u64_be();
        reader.read_u64_be(); // unknown 8 bytes
    }

    if (reader.failed()) return false;

    // 2. قراءة البيانات الوصفية (Metadata - Little Endian)
    if (version >= 7) {
        unity_version = reader.read_string_null();
    }
    if (version >= 8) {
        reader.read_i32_le(); // m_TargetPlatform
    }

    bool enable_type_tree = false;
    if (version >= 13) {
        enable_type_tree = reader.read_u8() != 0;
    }

    // 3. قراءة جدول الأنواع (m_Types)
    int32_t type_count = reader.read_i32_le();
    if (type_count < 0 || type_count > 100000 || reader.failed()) return false;

    std::vector<int32_t> type_class_ids(type_count);

    for (int32_t i = 0; i < type_count; ++i) {
        int32_t class_id = reader.read_i32_le();
        type_class_ids[i] = class_id;

        if (version >= 16) {
            reader.read_u8(); // m_IsStrippedType
        }
        int16_t script_type_index = -1;
        if (version >= 17) {
            script_type_index = reader.read_i16_le();
        }

        if (version >= 13) {
            if ((version < 16 && class_id < 0) || (version >= 16 && class_id == 114) || (version >= 17 && script_type_index >= 0)) {
                reader.read_bytes(16); // m_ScriptID
            }
            reader.read_bytes(16); // m_OldTypeHash
        }

        if (enable_type_tree) {
            int32_t node_count = reader.read_i32_le();
            int32_t string_buffer_size = reader.read_i32_le();
            size_t node_size = (version >= 19) ? 32 : 24;
            reader.read_bytes(node_count * node_size + string_buffer_size);

            if (version >= 21) {
                int32_t dep_count = reader.read_i32_le();
                if (dep_count > 0 && dep_count < 10000) {
                    reader.read_bytes(dep_count * 4);
                }
            }
        }
    }

    if (version >= 7 && version < 14) {
        reader.read_i32_le(); // m_BigIDEnabled
    }

    // 4. قراءة جدول الكائنات والمجسمات (m_Objects) مطابق 100% لـ AssetStudio
    int32_t object_count = reader.read_i32_le();
    if (object_count <= 0 || object_count > 2000000 || reader.failed()) return false;

    objects.resize(object_count);

    for (int32_t i = 0; i < object_count; ++i) {
        if (version >= 14) {
            reader.align(4);
            objects[i].path_id = reader.read_i64_le();
        } else {
            objects[i].path_id = static_cast<int64_t>(reader.read_i32_le());
        }

        if (version >= 22) {
            objects[i].byte_start = reader.read_u64_le();
        } else {
            objects[i].byte_start = static_cast<uint64_t>(reader.read_u32_le());
        }

        objects[i].byte_start += data_offset;
        objects[i].byte_size = reader.read_u32_le();
        objects[i].type_id = reader.read_i32_le();

        if (version < 16) {
            objects[i].class_id = reader.read_u16_le();
            reader.read_u16_le(); // isDestroyed
        } else {
            if (objects[i].type_id >= 0 && objects[i].type_id < static_cast<int32_t>(type_class_ids.size())) {
                objects[i].class_id = type_class_ids[objects[i].type_id];
            } else {
                objects[i].class_id = objects[i].type_id;
            }
        }

        if (version >= 11 && version < 17) {
            reader.read_u16_le(); // scriptTypeIndex
        }
        if (version >= 15 && version < 17) {
            reader.read_u8(); // stripped
        }

        objects[i].source_file = file_name;
        path_id_to_index[objects[i].path_id] = i;
    }

    return !reader.failed() && !objects.empty();
}
