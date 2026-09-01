#include "unity_studio.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void UnityStudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_bundle", "file_path"), &UnityStudio::load_bundle);
    ClassDB::bind_method(D_METHOD("get_internal_files"), &UnityStudio::get_internal_files);
    ClassDB::bind_method(D_METHOD("get_decompressed_size"), &UnityStudio::get_decompressed_size);
}

UnityStudio::UnityStudio() {}
UnityStudio::~UnityStudio() {}

bool UnityStudio::load_bundle(const String &file_path) {
    Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
    if (file.is_null()) {
        UtilityFunctions::printerr("Failed to open file: ", file_path);
        return false;
    }

    uint64_t len = file->get_length();
    PackedByteArray pba = file->get_buffer(len);

    is_loaded = current_archive.parse(pba.ptr(), len);
    return is_loaded;
}

PackedStringArray UnityStudio::get_internal_files() {
    PackedStringArray arr;
    if (!is_loaded) return arr;

    for (const auto& node : current_archive.directory_nodes) {
        arr.push_back(String(node.path.c_str()));
    }
    return arr;
}

int64_t UnityStudio::get_decompressed_size() {
    if (!is_loaded) return 0;
    return static_cast<int64_t>(current_archive.decompressed_data.size());
}
