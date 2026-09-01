#include "serialized_file.h"

bool SerializedFile::parse(const uint8_t* raw_data, size_t raw_size, const std::string& name) {
    if (!raw_data || raw_size < 20) return false;

    file_name = name;
    UnityReader reader(raw_data, raw_size);

    // 1. قراءة الترويسة (Header) - دائماً Big Endian
    metadata_size = reader.read_u32_be();
    file_size = static_cast<uint64_t>(reader.read_u32_be());
    version = reader.read_u32_be();
    data_offset = static_cast<uint64_t>(reader.read_u32_be());

    if (version >= 9) {
        endianess = reader.read_u8();
        reader.read_bytes(3); // m_Reserved
    }

    // مطابقة AssetStudio: إذا كان الإصدار 22 فما فوق (Unity 2020.3+ / 2021 / 2022 / 2023)
    if (version >= 22) {
        if (raw_size < 48) return false;
        metadata_size = reader.read_u32_be();
        file_size = reader.read_u64_be();
        data_offset = reader.read_u64_be();
        reader.read_u64_be(); // unknown 8 bytes
    }

    if (reader.failed()) return false;

    // 2. تحديد موقع الـ Metadata بدقة AssetStudio
    // في بعض حزم الألعاب القديمة، توضع البيانات الوصفية في نهاية الملف
    if (version < 9) {
        reader.set_position(file_size - metadata_size);
    }

    // 3. قراءة معلومات المحرك والمنصة
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

    // 4. قراءة جدول الأنواع (m_Types) مطابق لـ AssetStudio
    int32_t type_count = reader.read_i32_le();
    if (type_count < 0 || type_count > 100000) return false;

    for (int32_t i = 0; i < type_count; ++i) {
        int32_t class_id = reader.read_i32_le();
        
        if (version >= 16) {
            reader.read_u8(); // m_IsStrippedType
        }
        
        int16_t script_type_index = -1;
        if (version >= 17) {
            script_type_index = reader.read_i16_le();
        }

        if (version >= 13) {
            if ((version < 16 && class_id < 0) || (version >= 16 && class_id == 114) || (version >= 17 && script_type_index >= 0)) {
                reader.read_bytes(16); // m_ScriptID (Hash128)
            }
            reader.read_bytes(16); // m_OldTypeHash (Hash128)
        }

        // قراءة الـ TypeTree Blob إذا كانت مدمجة
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

    // 5. قراءة الـ RefTypes المضافة في Unity 20+
    if (version >= 20) {
        int32_t ref_type_count = reader.read_i32_le();
        for (int32_t i = 0; i < ref_type_count; ++i) {
            int32_t class_id = reader.read_i32_le();
            if (version >= 16) reader.read_u8();
            int16_t script_type_index = (version >= 17) ? reader.read_i16_le() : -1;
            
            if (version >= 13) {
                if (script_type_index >= 0 || class_id == 114) reader.read_bytes(16);
                reader.read_bytes(16);
            }
            if (enable_type_tree) {
                int32_t node_count = reader.read_i32_le();
                int32_t string_buffer_size = reader.read_i32_le();
                size_t node_size = (version >= 19) ? 32 : 24;
                reader.read_bytes(node_count * node_size + string_buffer_size);
            }
            reader.read_string_null(); // m_ClassName
            reader.read_string_null(); // m_Namespace
            reader.read_string_null(); // m_AssemblyName
            
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

    // 6. قراءة جدول الكائنات والمجسمات (m_Objects)
    if (version >= 14) reader.align(4);
    int32_t object_count = reader.read_i32_le();

    if (object_count <= 0 || object_count > 2000000 || reader.failed()) return false;

    objects.resize(object_count);

    for (int32_t i = 0; i < object_count; ++i) {
        if (version >= 14) reader.align(4);

        // قراءة PathID
        if (version >= 14) {
            objects[i].path_id = reader.read_i64_le();
        } else {
            objects[i].path_id = static_cast<int64_t>(reader.read_i32_le());
        }

        // قراءة موقع وحجم الكائن
        if (version >= 22) {
            objects[i].byte_start = reader.read_u64_le();
        } else {
            objects[i].byte_start = static_cast<uint64_t>(reader.read_u32_le());
        }
        
        objects[i].byte_start += data_offset;
        objects[i].byte_size = reader.read_u32_le();
        objects[i].type_id = reader.read_i32_le();

        // في AssetStudio: إذا كان الإصدار أقل من 16، الـ ClassID يقرأ من حقل منفصل
        if (version < 16) {
            objects[i].class_id = reader.read_u16_le();
            reader.read_u16_le(); // m_IsDestroyed
        } else {
            objects[i].class_id = objects[i].type_id;
        }

        if (version >= 11 && version < 17) {
            reader.read_u16_le(); // m_ScriptTypeIndex
        }
        if (version >= 15 && version < 17) {
            reader.read_u8(); // m_Stripped
        }

        objects[i].source_file = file_name;
        path_id_to_index[objects[i].path_id] = i;
    }

    return !reader.failed() && !objects.empty();
}
