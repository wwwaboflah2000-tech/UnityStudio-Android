#include "unity_objects.h"

// === 1. قراءة مجسمات Unity بدقة تسلسل Format Version (v17 - v22) ===
bool UnityMesh::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 24) return false;
    UnityReader reader(data, size);

    name = reader.read_string_aligned();

    // 1. SubMeshes
    int32_t submesh_count = reader.read_i32_le();
    if (submesh_count > 0 && submesh_count < 1000) {
        for (int i = 0; i < submesh_count; ++i) {
            reader.read_bytes(version >= 17 ? 48 : 36);
        }
    }

    // 2. BlendShapes (m_Shapes)
    if (version >= 9) {
        int32_t num_verts = reader.read_i32_le();
        if (num_verts > 0) reader.read_bytes(num_verts * 40);
        int32_t num_shapes = reader.read_i32_le();
        if (num_shapes > 0) reader.read_bytes(num_shapes * 36);
        int32_t num_channels = reader.read_i32_le();
        if (num_channels > 0) reader.read_bytes(num_channels * 36);
        int32_t num_weights = reader.read_i32_le();
        if (num_weights > 0) reader.read_bytes(num_weights * 4);
    }

    // 3. BindPoses
    if (version >= 9) {
        int32_t num_bind_poses = reader.read_i32_le();
        if (num_bind_poses > 0) reader.read_bytes(num_bind_poses * 64);
        int32_t num_bone_hashes = reader.read_i32_le();
        if (num_bone_hashes > 0) reader.read_bytes(num_bone_hashes * 4);
        reader.read_u32_le(); // root_bone_name_hash
    }

    // 4. BonesAABB
    if (version >= 19) {
        int32_t num_bones_aabb = reader.read_i32_le();
        if (num_bones_aabb > 0) reader.read_bytes(num_bones_aabb * 24);
        int32_t num_var_weights = reader.read_i32_le();
        if (num_var_weights > 0) reader.read_bytes(num_var_weights * 4);
    }

    // 5. MeshCompression & StreamData
    reader.read_u8(); // mesh_compression
    if (version >= 17) {
        reader.read_u64_le(); // stream offset
        reader.read_u32_le(); // stream size
        reader.read_string_aligned(); // stream path
    }

    // 6. Index Buffer (المثلثات)
    int32_t index_data_size = reader.read_i32_le();
    if (index_data_size > 0 && index_data_size <= static_cast<int32_t>(size - reader.get_position())) {
        int32_t num_indices = index_data_size / 2; // 16-bit indices
        indices.resize(num_indices);
        for (int i = 0; i < num_indices; ++i) {
            indices[i] = reader.read_u16_le();
        }
    }
    reader.align(4);

    // 7. Skin
    if (version >= 17) {
        int32_t num_skin = reader.read_i32_le();
        if (num_skin > 0) reader.read_bytes(num_skin * 32);
    }

    // 8. VertexData (النقاط ثلاثية الأبعاد)
    reader.read_u32_le(); // current_channels
    int32_t vertex_count = reader.read_i32_le();
    
    if (version >= 17) {
        int32_t channel_count = reader.read_i32_le();
        ChannelInfo channels[16];
        for (int i = 0; i < channel_count && i < 16; ++i) {
            channels[i].stream = reader.read_u8();
            channels[i].offset = reader.read_u8();
            channels[i].format = reader.read_u8();
            channels[i].dimension = reader.read_u8();
        }
        
        reader.align(4);
        int32_t vertex_data_size = reader.read_i32_le();
        if (vertex_count > 0 && vertex_data_size > 0 && vertex_data_size <= static_cast<int32_t>(size - reader.get_position())) {
            std::vector<uint8_t> vdata = reader.read_bytes(vertex_data_size);
            int32_t stride = vertex_data_size / vertex_count;
            
            if (stride >= 12) {
                vertices.resize(vertex_count);
                for (int i = 0; i < vertex_count; ++i) {
                    float vx, vy, vz;
                    memcpy(&vx, vdata.data() + (i * stride) + channels[0].offset, 4);
                    memcpy(&vy, vdata.data() + (i * stride) + channels[0].offset + 4, 4);
                    memcpy(&vz, vdata.data() + (i * stride) + channels[0].offset + 8, 4);
                    vertices[i] = Vector3(vx, vy, -vz);
                }
            }
        }
    }

    return (!vertices.empty() && !indices.empty());
}

Ref<ArrayMesh> UnityMesh::create_godot_mesh() {
    if (vertices.empty() || indices.empty()) return Ref<ArrayMesh>();

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

// === 2. قراءة GameObject و Transform ===
bool UnityGameObject::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 8) return false;
    UnityReader reader(data, size);

    int32_t num_components = reader.read_i32_le();
    if (num_components < 0 || num_components > 1000) return false;

    components.resize(num_components);
    for (int i = 0; i < num_components; ++i) {
        components[i].file_id = reader.read_i32_le();
        components[i].path_id = (version >= 14) ? reader.read_i64_le() : static_cast<int64_t>(reader.read_i32_le());
        if (i == 0) transform_path_id = components[i].path_id;
    }

    reader.read_i32_le(); // layer
    name = reader.read_string_aligned();
    return !reader.failed();
}

bool UnityTransform::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 28) return false;
    UnityReader reader(data, size);

    reader.read_i32_le(); // game_object file_id
    game_object_path_id = (version >= 14) ? reader.read_i64_le() : static_cast<int64_t>(reader.read_i32_le());

    float rx = reader.read_float_le();
    float ry = reader.read_float_le();
    float rz = reader.read_float_le();
    float rw = reader.read_float_le();
    local_rotation = Quaternion(-rx, -ry, rz, rw);

    float px = reader.read_float_le();
    float py = reader.read_float_le();
    float pz = reader.read_float_le();
    local_position = Vector3(px, py, -pz);

    float sx = reader.read_float_le();
    float sy = reader.read_float_le();
    float sz = reader.read_float_le();
    local_scale = Vector3(sx, sy, sz);

    int32_t num_children = reader.read_i32_le();
    if (num_children > 0 && num_children < 10000) {
        children_path_ids.resize(num_children);
        for (int i = 0; i < num_children; ++i) {
            reader.read_i32_le(); // child file_id
            children_path_ids[i] = (version >= 14) ? reader.read_i64_le() : static_cast<int64_t>(reader.read_i32_le());
        }
    }

    reader.read_i32_le(); // father file_id
    father_path_id = (version >= 14) ? reader.read_i64_le() : static_cast<int64_t>(reader.read_i32_le());

    return !reader.failed();
}

// === 3. قراءة الصور والنصوص ===
bool UnityTexture2D::parse(const uint8_t* data, size_t size, int version) {
    if (!data || size < 16) return false;
    UnityReader reader(data, size);
    name = reader.read_string_aligned();
    width = reader.read_i32_le();
    height = reader.read_i32_le();
    reader.read_i32_le();
    texture_format = reader.read_i32_le();
    if (version >= 52) reader.read_i32_le();

    reader.align(4);
    int32_t image_data_size = reader.read_i32_le();
    if (image_data_size > 0 && image_data_size <= static_cast<int32_t>(size - reader.get_position())) {
        image_data = reader.read_bytes(image_data_size);
    }
    return (width > 0 && height > 0);
}

Ref<Image> UnityTexture2D::create_godot_image() {
    if (width <= 0 || height <= 0 || image_data.empty()) return Ref<Image>();
    PackedByteArray pba;
    pba.resize(image_data.size());
    memcpy(pba.ptrw(), image_data.data(), image_data.size());

    Ref<Image> img = memnew(Image);
    if (texture_format == 4 || texture_format == 5) img->set_data(width, height, false, Image::FORMAT_RGBA8, pba);
    else if (texture_format == 3) img->set_data(width, height, false, Image::FORMAT_RGB8, pba);
    else if (texture_format == 10) img->set_data(width, height, false, Image::FORMAT_DXT1, pba);
    else if (texture_format == 12) img->set_data(width, height, false, Image::FORMAT_DXT5, pba);
    else if (texture_format == 34) img->set_data(width, height, false, Image::FORMAT_ETC, pba);
    else if (texture_format == 45) img->set_data(width, height, false, Image::FORMAT_ETC2_RGB8, pba);
    else if (texture_format == 47) img->set_data(width, height, false, Image::FORMAT_ETC2_RGBA8, pba);
    else if (texture_format == 48 || texture_format == 54) img->set_data(width, height, false, Image::FORMAT_ASTC_4x4, pba);
    else img->set_data(width, height, false, Image::FORMAT_RGBA8, pba);

    if (img->is_empty()) return Ref<Image>();
    img->flip_y();
    return img;
}

bool UnityTextAsset::parse(const uint8_t* data, size_t size) {
    if (!data || size < 4) return false;
    UnityReader reader(data, size);
    name = reader.read_string_aligned();
    script = reader.read_string_aligned();
    return !reader.failed();
}
