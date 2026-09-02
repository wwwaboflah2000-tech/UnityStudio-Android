#ifndef UNITY_STUDIO_H
#define UNITY_STUDIO_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

#include "unity_fs.h"
#include "serialized_file.h"
#include "unity_objects.h"

namespace godot {

class UnityStudio : public RefCounted {
    GDCLASS(UnityStudio, RefCounted)

private:
    UnityFSArchive current_archive;
    std::vector<SerializedFile> loaded_files;
    PackedByteArray raw_master_buffer;
    bool is_loaded = false;

    const uint8_t* get_object_data(int64_t path_id, uint32_t &out_size, int &out_version);

protected:
    static void _bind_methods();

public:
    UnityStudio();
    ~UnityStudio();

    bool load_bundle(const String &file_path);
    bool load_raw_data(const PackedByteArray &pba, const String &name);
    void clear_all();

    int64_t get_object_count();
    Array get_game_objects_list();
    Array get_scene_hierarchy();

    String get_text_asset(int64_t path_id);
    Ref<Image> get_texture_image(int64_t path_id);
    Ref<ArrayMesh> get_mesh(int64_t path_id);
};

} // namespace godot

#endif
