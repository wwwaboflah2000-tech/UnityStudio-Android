#include "unity_studio.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void UnityStudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_bundle", "file_path"), &UnityStudio::load_bundle);
    ClassDB::bind_method(D_METHOD("load_raw_data", "data", "name"), &UnityStudio::load_raw_data);
    ClassDB::bind_method(D_METHOD("clear_all"), &UnityStudio::clear_all);
    ClassDB::bind_method(D_METHOD("get_object_count"), &UnityStudio::get_object_count);
    ClassDB::bind_method(D_METHOD("get_game_objects_list"), &UnityStudio::get_game_objects_list);

    // ربط دوال الفك مع Godot
    ClassDB::bind_method(D_METHOD("get_text_asset", "path_id"), &UnityStudio::get_text_asset);
    ClassDB::bind_method(D_METHOD("get_texture_image", "path_id"), &UnityStudio::get_texture_image);
    ClassDB::bind_method(D_METHOD("get_audio_stream", "path_id"), &UnityStudio::get_audio_stream);
    ClassDB::bind_method(D_METHOD("get_mesh", "path_id"), &UnityStudio::get_mesh);
}

UnityStudio::UnityStudio() {}
UnityStudio::~UnityStudio() {}

void UnityStudio::clear_all() {
    loaded_files.clear();
    raw_master_buffer.clear();
    is_loaded = false;
}

const uint8_t* UnityStudio::get_object_data(int64_t path_id, uint32_t &out_size, int &out_version) {
    for (const auto& sfile : loaded_files) {
        auto it = sfile.path_id_to_index.find(path_id);
        if (it != sfile.path_id_to_index.end()) {
            const auto& obj = sfile.objects[it->second];
            out_size = obj.byte_size;
            out_version = sfile.version;
            
            if (obj.byte_start + obj.byte_size <= static_cast<uint64_t>(raw_master_buffer.size())) {
                return raw_master_buffer.ptr() + obj.byte_start;
            }
        }
    }
    out_size = 0;
    return nullptr;
}

bool UnityStudio::load_raw_data(const PackedByteArray &pba, const String &name) {
    uint64_t len = pba.size();
    if (len < 16) return false;
    raw_master_buffer = pba;
    const uint8_t* data_ptr = raw_master_buffer.ptr();

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

int64_t UnityStudio::get_object_count() {
    int64_t total = 0;
    for (const auto& sfile : loaded_files) total += sfile.objects.size();
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

// ⚡ فك النصوص والشيدر ⚡
String UnityStudio::get_text_asset(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return "";

    UnityTextAsset text;
    if (text.parse(data, size)) {
        return String::utf8(text.script.c_str());
    }
    return "";
}

// ⚡ فك وتوليد الصور (Texture2D) ⚡
Ref<Image> UnityStudio::get_texture_image(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return Ref<Image>();

    UnityTexture2D tex;
    if (tex.parse(data, size, version)) {
        return tex.create_godot_image();
    }
    return Ref<Image>();
}

// ⚡ فك وتوليد الصوتيات (AudioClip) ⚡
Ref<AudioStreamWAV> UnityStudio::get_audio_stream(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return Ref<AudioStreamWAV>();

    UnityAudioClip audio;
    if (audio.parse(data, size, version)) {
        return audio.create_godot_audio();
    }
    return Ref<AudioStreamWAV>();
}

// ⚡ فك وتوليد شبكة المجسمات (3D Mesh) ⚡
Ref<ArrayMesh> UnityStudio::get_mesh(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return Ref<ArrayMesh>();

    UnityMesh mesh;
    if (mesh.parse(data, size, version)) {
        return mesh.create_godot_mesh();
    }
    return Ref<ArrayMesh>();
}
