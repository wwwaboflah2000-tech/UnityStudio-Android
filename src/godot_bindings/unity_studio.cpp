#include "unity_studio.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void UnityStudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_bundle", "file_path"), &UnityStudio::load_bundle);
    ClassDB::bind_method(D_METHOD("get_internal_files"), &UnityStudio::get_internal_files);
    ClassDB::bind_method(D_METHOD("get_object_count"), &UnityStudio::get_object_count);
    ClassDB::bind_method(D_METHOD("get_game_objects_list"), &UnityStudio::get_game_objects_list);
}

UnityStudio::UnityStudio() {}
UnityStudio::~UnityStudio() {}

bool UnityStudio::load_bundle(const String &file_path) {
    Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
    if (file.is_null()) return false;

    uint64_t len = file->get_length();
    if (len < 16) return false;

    PackedByteArray pba = file->get_buffer(len);
    const uint8_t* data_ptr = pba.ptr();

    loaded_files.clear();
    is_loaded = false;

    // فحص هل هو UnityFS أم ملف أصول مباشر
    std::string signature;
    for (size_t i = 0; i < 7 && i < len; ++i) {
        if (data_ptr[i] == '\0') break;
        signature += static_cast<char>(data_ptr[i]);
    }

    if (signature == "UnityFS") {
        // فك ضغط الحزمة بالكامل
        if (current_archive.parse(data_ptr, len)) {
            // مسح جميع الملفات الداخلية بلا استثناء
            for (const auto& node : current_archive.directory_nodes) {
                if (node.size == 0) continue;
                
                const uint8_t* node_data = current_archive.decompressed_data.data() + node.offset;
                SerializedFile sfile;
                
                // محاولة قراءة كل ملف كـ SerializedFile
                if (sfile.parse(node_data, node.size, node.path)) {
                    loaded_files.push_back(sfile);
                }
            }
            is_loaded = !loaded_files.empty();
        }
    } else {
        // ملف أصول مباشر (مثل sharedassets0.assets أو level0)
        SerializedFile sfile;
        String base_name = file_path.get_file();
        if (sfile.parse(data_ptr, len, base_name.utf8().get_data())) {
            loaded_files.push_back(sfile);
            is_loaded = true;
        }
    }

    return is_loaded;
}

PackedStringArray UnityStudio::get_internal_files() {
    PackedStringArray arr;
    for (const auto& sfile : loaded_files) {
        arr.push_back(String(sfile.file_name.c_str()));
    }
    return arr;
}

int64_t UnityStudio::get_object_count() {
    int64_t total = 0;
    for (const auto& sfile : loaded_files) {
        total += sfile.objects.size();
    }
    return total;
}

Array UnityStudio::get_game_objects_list() {
    Array result;
    for (const auto& sfile : loaded_files) {
        for (const auto& obj : sfile.objects) {
            Dictionary d;
            d["path_id"] = obj.path_id;
            d["class_id"] = obj.class_id;
            d["byte_size"] = obj.byte_size;
            d["source_file"] = String(obj.source_file.c_str());
            result.push_back(d);
        }
    }
    return result;
}
