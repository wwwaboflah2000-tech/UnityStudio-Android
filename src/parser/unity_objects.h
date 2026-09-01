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
#include <godot_cpp/classes/standard_material3d.hpp>

using namespace godot;

// 1. محلل النصوص والشيدر
class UnityTextAsset {
public:
    std::string name;
    std::string script;

    bool parse(const uint8_t* data, size_t size);
};

// 2. محلل الصور والخامات (Texture2D)
class UnityTexture2D {
public:
    std::string name;
    int32_t width = 0;
    int32_t height = 0;
    int32_t texture_format = 0;
    int32_t mip_count = 1;
    std::vector<uint8_t> image_data;

    bool parse(const uint8_t* data, size_t size, int version);
    Ref<Image> create_godot_image();
};

// 3. محلل الصوتيات (AudioClip)
class UnityAudioClip {
public:
    std::string name;
    int32_t frequency = 44100;
    int32_t channels = 2;
    int32_t bits_per_sample = 16;
    std::vector<uint8_t> audio_data;

    bool parse(const uint8_t* data, size_t size, int version);
    Ref<AudioStreamWAV> create_godot_audio();
};

// 4. محلل المجسمات ثلاثية الأبعاد (Mesh)
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

#endif
