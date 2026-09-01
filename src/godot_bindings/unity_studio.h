#ifndef UNITY_STUDIO_H
#define UNITY_STUDIO_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

#include "unity_fs.h"
#include "serialized_file.h"

namespace godot {

class UnityStudio : public RefCounted {
    GDCLASS(UnityStudio, RefCounted)

private:
    UnityFSArchive current_archive;
    std::vector<SerializedFile> loaded_files; // جميع الملفات المفككة
    bool is_loaded = false;

protected:
    static void _bind_methods();

public:
    UnityStudio();
    ~UnityStudio();

    bool load_bundle(const String &file_path);
    PackedStringArray get_internal_files();
    int64_t get_object_count();
    Array get_game_objects_list();
};

} // namespace godot

#endif
