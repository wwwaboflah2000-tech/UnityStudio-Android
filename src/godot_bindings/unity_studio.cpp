#include "unity_studio.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void UnityStudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_bundle", "file_path"), &UnityStudio::load_bundle);
    ClassDB::bind_method(D_METHOD("load_raw_data", "data", "name"), &UnityStudio::load_raw_data);
    ClassDB::bind_method(D_METHOD("clear_all"), &UnityStudio::clear_all);
    ClassDB::bind_method(D_METHOD("get_internal_files"), &UnityStudio::get_internal_files);
    ClassDB::bind_method(D_METHOD("get_object_count"), &UnityStudio::get_object_count);
    ClassDB::bind_method(D_METHOD("get_game_objects_list"), &UnityStudio::get_game_objects_list);
}

UnityStudio::UnityStudio() {}
UnityStudio::~UnityStudio() {}

void UnityStudio::clear_all() {
    loaded_files.clear();
    is_loaded = false;
}

bool UnityStudio::load_raw_data(const PackedByteArray &pba, const String &name) {
    uint64_t len = pba.size();
    if (len < 16) return false;
    const uint8_t* data_ptr = pba.ptr();

    std::string signature;
    for (size_t i = 0; i < 7 && i < len; ++i) {
        if (data_ptr[i] == '\0') break;
        signature += static_cast<char>(data_ptr[i]);
    }

    if (signature == "UnityFS") {
        UnityFSArchive archive;
        if (archive.parse(data_ptr, len)) {
            for (const auto& node : archive.directory_nodes) {
                if (node.size == 0) continue;
                const uint8_t* node_data = archive.decompressed_data.data() + node.offset;
                SerializedFile sfile;
                if (sfile.parse(node_data, node.size, node.path)) {
                    loaded_files.push_back(sfile);
                }
            }
            is_loaded = !loaded_files.empty();
            return is_loaded;
        }
    } else {
        SerializedFile sfile;
        if (sfile.parse(data_ptr, len, name.utf8().get_data())) {
            loaded_files.push_back(sfile);
            is_loaded = true;
            return true;
        }
    }
    return false;
}

bool UnityStudio::load_bundle(const String &file_path) {
    Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
    if (file.is_null()) return false;

    uint64_t len = file->get_length();
    if (len < 16) return false;

    PackedByteArray pba = file->get_buffer(len);
    clear_all();
    return load_raw_data(pba, file_path.get_file());
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
