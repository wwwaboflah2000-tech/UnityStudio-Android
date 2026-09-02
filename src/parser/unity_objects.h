#ifndef UNITY_OBJECTS_H
#define UNITY_OBJECTS_H

#include "unity_reader.h"
#include <vector>
#include <string>
#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

using namespace godot;

struct ChannelInfo {
    uint8_t stream = 0;
    uint8_t offset = 0;
    uint8_t format = 0;
    uint8_t dimension = 0;
};

// 1. محلل المجسمات ثلاثية الأبعاد الحقيقي (Mesh.cs من AssetStudio)
class UnityMesh {
public:
    std::string name;
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Vector2> uvs;
    std::vector<int32_t> indices;

    bool parse(const uint8_t* data, size_t size, int version);
    Ref<ArrayMesh> create_godot_mesh();
};

// 2. محلل شجرة الكائنات (GameObject & Transform)
struct PPtr {
    int32_t file_id = 0;
    int64_t path_id = 0;
};

class UnityGameObject {
public:
    std::string name;
    std::vector<PPtr> components;
    int64_t transform_path_id = 0;
    int64_t mesh_path_id = 0;

    bool parse(const uint8_t* data, size_t size, int version);
};

class UnityTransform {
public:
    int64_t game_object_path_id = 0;
    Vector3 local_position = Vector3(0, 0, 0);
    Quaternion local_rotation = Quaternion(0, 0, 0, 1);
    Vector3 local_scale = Vector3(1, 1, 1);
    int64_t father_path_id = 0;
    std::vector<int64_t> children_path_ids;

    bool parse(const uint8_t* data, size_t size, int version);
};

// 3. محلل الصور
class UnityTexture2D {
public:
    std::string name;
    int32_t width = 0;
    int32_t height = 0;
    int32_t texture_format = 0;
    std::vector<uint8_t> image_data;

    bool parse(const uint8_t* data, size_t size, int version);
    Ref<Image> create_godot_image();
};

// 4. محلل النصوص
class UnityTextAsset {
public:
    std::string name;
    std::string script;

    bool parse(const uint8_t* data, size_t size);
};

#endif
