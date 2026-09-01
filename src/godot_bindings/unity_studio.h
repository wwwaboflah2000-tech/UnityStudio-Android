#ifndef UNITY_STUDIO_H
#define UNITY_STUDIO_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "unity_fs.h"

namespace godot {

class UnityStudio : public RefCounted {
    GDCLASS(UnityStudio, RefCounted)

private:
    UnityFSArchive current_archive;
    bool is_loaded = false;

protected:
    static void _bind_methods();

public:
    UnityStudio();
    ~UnityStudio();

    bool load_bundle(const String &file_path);
    PackedStringArray get_internal_files();
    int64_t get_decompressed_size();
};

} // namespace godot

#endif
