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
    PackedByteArray pba = file->get_buffer(len);

    is_loaded = current_archive.parse(pba.ptr(), len);
    if (!is_loaded) return false;

    // البحث عن أول ملف أصول مفكوك وقراءته
    for (const auto& node : current_archive.directory_nodes) {
        if (node.path.find(".assets") != std::string::npos || node.path.find("CAB-") != std::string::npos) {
            const uint8_t* assets_data = current_archive.decompressed_data.data() + node.offset;
            current_serialized_file.parse(assets_data, node.size);
            break;
        }
    }

    return true;
}

PackedStringArray UnityStudio::get_internal_files() {
    PackedStringArray arr;
    if (!is_loaded) return arr;

    for (const auto& node : current_archive.directory_nodes) {
        arr.push_back(String(node.path.c_str()));
    }
    return arr;
}

int64_t UnityStudio::get_object_count() {
    return static_cast<int64_t>(current_serialized_file.objects.size());
}

Array UnityStudio::get_game_objects_list() {
    Array result;
    for (const auto& obj : current_serialized_file.objects) {
        Dictionary d;
        d["path_id"] = obj.path_id;
        d["class_id"] = obj.class_id;
        d["byte_size"] = obj.byte_size;
        result.push_back(d);
    }
    return result;
}
