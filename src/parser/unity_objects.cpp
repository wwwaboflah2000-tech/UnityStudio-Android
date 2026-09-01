#include "unity_objects.h"
#include <godot_cpp/variant/utility_functions.hpp>

// === 1. TextAsset Parser ===
bool UnityTextAsset::parse(const uint8_t* data, size_t size) {
    if (!data || size < 4) return false;
    UnityReader reader(data, size);

    name = reader.read_string_aligned();
    script = reader.read_string_aligned();
    return !reader.failed();
}

// === 2. Texture2D Parser ===
bool UnityTexture2D::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 16) return false;
    UnityReader reader(data, size);

    name = reader.read_string_aligned();
    width = reader.read_i32_le();
    height = reader.read_i32_le();
    int32_t complete_image_size = reader.read_i32_le();
    texture_format = reader.read_i32_le();

    if (version >= 52) { // 5.2+
        mip_count = reader.read_i32_le();
    }

    // البحث عن مصفوفة البكسلات
    reader.align(4);
    int32_t image_data_size = reader.read_i32_le();
    if (image_data_size > 0 && image_data_size <= static_cast<int32_t>(size - reader.get_position())) {
        image_data = reader.read_bytes(image_data_size);
    }

    return (width > 0 && height > 0);
}

Ref<Image> UnityTexture2D::create_godot_image() {
    if (width <= 0 || height <= 0 || image_data.empty()) {
        return Ref<Image>();
    }

    PackedByteArray pba;
    pba.resize(image_data.size());
    memcpy(pba.ptrw(), image_data.data(), image_data.size());

    Ref<Image> img = memnew(Image);

    // التعرف على صيغ صور Unity وتحويلها لصيغ Godot
    switch (texture_format) {
        case 1: // Alpha8
            img->set_data(width, height, false, Image::FORMAT_L8, pba);
            break;
        case 3: // RGB24
            img->set_data(width, height, false, Image::FORMAT_RGB8, pba);
            break;
        case 4: // RGBA32
        case 5: // ARGB32
            img->set_data(width, height, false, Image::FORMAT_RGBA8, pba);
            break;
        case 7: // RGB565
            img->set_data(width, height, false, Image::FORMAT_RGB565, pba);
            break;
        case 10: // DXT1 / BC1
            img->set_data(width, height, false, Image::FORMAT_DXT1, pba);
            break;
        case 12: // DXT5 / BC3
            img->set_data(width, height, false, Image::FORMAT_DXT5, pba);
            break;
        case 34: // ETC_RGB4
            img->set_data(width, height, false, Image::FORMAT_ETC, pba);
            break;
        case 45: // ETC2_RGB
            img->set_data(width, height, false, Image::FORMAT_ETC2_RGB8, pba);
            break;
        case 47: // ETC2_RGBA8
            img->set_data(width, height, false, Image::FORMAT_ETC2_RGBA8, pba);
            break;
        case 48: // ASTC_RGB_4x4
        case 54: // ASTC_RGBA_4x4
            img->set_data(width, height, false, Image::FORMAT_ASTC_4x4, pba);
            break;
        default:
            // محاولة افتراضية كـ RGBA8
            if (image_data.size() >= static_cast<size_t>(width * height * 4)) {
                img->set_data(width, height, false, Image::FORMAT_RGBA8, pba);
            } else if (image_data.size() >= static_cast<size_t>(width * height * 3)) {
                img->set_data(width, height, false, Image::FORMAT_RGB8, pba);
            }
            break;
    }

    if (img->is_empty()) return Ref<Image>();

    // قلب الصورة عمودياً (لأن Unity تخزن الصور مقلوبة من الأسفل للأعلى)
    img->flip_y();
    return img;
}

// === 3. AudioClip Parser ===
bool UnityAudioClip::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 16) return false;
    UnityReader reader(data, size);

    name = reader.read_string_aligned();
    int32_t format = reader.read_i32_le();
    int32_t type = reader.read_i32_le();
    bool is_3d = reader.read_u8() != 0;
    reader.align(4);
    channels = reader.read_i32_le();
    frequency = reader.read_i32_le();

    if (channels <= 0) channels = 2;
    if (frequency <= 0) frequency = 44100;

    // قراءة البيانات الصوتية المدمجة
    reader.align(4);
    int32_t data_size = reader.read_i32_le();
    if (data_size > 0 && data_size <= static_cast<int32_t>(size - reader.get_position())) {
        audio_data = reader.read_bytes(data_size);
    }
    return true;
}

Ref<AudioStreamWAV> UnityAudioClip::create_godot_audio() {
    Ref<AudioStreamWAV> wav = memnew(AudioStreamWAV);
    wav->set_mix_rate(frequency);
    wav->set_stereo(channels > 1);
    wav->set_format(AudioStreamWAV::FORMAT_16_BITS);

    if (!audio_data.empty()) {
        PackedByteArray pba;
        pba.resize(audio_data.size());
        memcpy(pba.ptrw(), audio_data.data(), audio_data.size());
        wav->set_data(pba);
    }
    return wav;
}

// === 4. Mesh Parser (شبكة المجسمات 3D) ===
bool UnityMesh::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 24) return false;
    UnityReader reader(data, size);

    name = reader.read_string_aligned();

    // قراءة SubMeshes
    int32_t submesh_count = reader.read_i32_le();
    if (submesh_count > 0 && submesh_count < 1000) {
        for (int i = 0; i < submesh_count; ++i) {
            reader.read_bytes(version >= 2017 ? 48 : 36); // SubMesh struct
        }
    }

    // قراءة الـ Index Buffer (المثلثات)
    reader.align(4);
    int32_t index_data_size = reader.read_i32_le();
    if (index_data_size > 0 && index_data_size < static_cast<int32_t>(size)) {
        int32_t num_indices = index_data_size / 2; // 16-bit indices
        indices.resize(num_indices);
        for (int i = 0; i < num_indices; ++i) {
            indices[i] = reader.read_u16_le();
        }
    }

    // قراءة الـ Vertex Buffer (النقاط ثلاثية الأبعاد)
    reader.align(4);
    int32_t vertex_count = reader.read_i32_le();
    if (vertex_count > 0 && vertex_count < 500000) {
        vertices.resize(vertex_count);
        for (int i = 0; i < vertex_count; ++i) {
            float vx = reader.read_float_le();
            float vy = reader.read_float_le();
            float vz = reader.read_float_le();
            // تحويل المحاور من Unity (Left-Handed) إلى Godot (Right-Handed)
            vertices[i] = Vector3(vx, vy, -vz);
        }
    }

    return (!vertices.empty() && !indices.empty());
}

Ref<ArrayMesh> UnityMesh::create_godot_mesh() {
    if (vertices.empty() || indices.empty()) {
        return Ref<ArrayMesh>();
    }

    Ref<ArrayMesh> mesh = memnew(ArrayMesh);
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    PackedVector3Array godot_vertices;
    godot_vertices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        godot_vertices[i] = vertices[i];
    }
    arrays[Mesh::ARRAY_VERTEX] = godot_vertices;

    PackedInt32Array godot_indices;
    godot_indices.resize(indices.size());
    // عكس ترتيب المثلثات حتى لا تظهر الأسطح شفافة
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 < indices.size()) {
            godot_indices[i] = indices[i];
            godot_indices[i + 1] = indices[i + 2];
            godot_indices[i + 2] = indices[i + 1];
        }
    }
    arrays[Mesh::ARRAY_INDEX] = godot_indices;

    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return mesh;
}
