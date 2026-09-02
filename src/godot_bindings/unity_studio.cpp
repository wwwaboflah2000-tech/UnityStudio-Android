#include "unity_studio.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <unordered_map>
#include <unordered_set>

using namespace godot;

void UnityStudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_bundle", "file_path"), &UnityStudio::load_bundle);
    ClassDB::bind_method(D_METHOD("load_raw_data", "data", "name"), &UnityStudio::load_raw_data);
    ClassDB::bind_method(D_METHOD("clear_all"), &UnityStudio::clear_all);
    ClassDB::bind_method(D_METHOD("get_object_count"), &UnityStudio::get_object_count);
    ClassDB::bind_method(D_METHOD("get_game_objects_list"), &UnityStudio::get_game_objects_list);
    ClassDB::bind_method(D_METHOD("get_scene_hierarchy"), &UnityStudio::get_scene_hierarchy);

    ClassDB::bind_method(D_METHOD("get_text_asset", "path_id"), &UnityStudio::get_text_asset);
    ClassDB::bind_method(D_METHOD("get_texture_image", "path_id"), &UnityStudio::get_texture_image);
    ClassDB::bind_method(D_METHOD("get_mesh", "path_id"), &UnityStudio::get_mesh);
    ClassDB::bind_method(D_METHOD("build_game_object_model", "go_path_id"), &UnityStudio::build_game_object_model);
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
            
            // حساب الإزاحة الإجمالية للملف داخل الـ Buffer الرئيسي بناءً على اسم الملف
            size_t global_offset = 0;
            // في هذا التحديث، raw_master_buffer يحتوي على الملفات بشكل تسلسلي، لكن للتبسيط:
            // إذا كان الكائن ضمن حدود البايتات المرفقة:
            if (obj.byte_start + obj.byte_size <= static_cast<uint64_t>(raw_master_buffer.size())) {
                return raw_master_buffer.ptr() + obj.byte_start;
            }
        }
    }
    out_size = 0;
    return nullptr;
}

// تعديل بسيط: `load_raw_data` الآن تقوم بإضافة (Append) البيانات بدلاً من مسحها لتدعم مجلدات كاملة
bool UnityStudio::load_raw_data(const PackedByteArray &pba, const String &name) {
    uint64_t len = pba.size();
    if (len < 16) return false;
    
    // دمج البيانات الجديدة في الذاكرة الرئيسية
    size_t old_size = raw_master_buffer.size();
    raw_master_buffer.append_array(pba);
    const uint8_t* data_ptr = raw_master_buffer.ptr() + old_size;

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
                // ضبط data_offset ليكون نسبة للـ Buffer الكلي
                sfile.data_offset += old_size;
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
            // ضبط البداية لأننا أضفناها في نهاية الذاكرة
            for(auto& obj : sfile.objects) {
                obj.byte_start += old_size;
            }
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

Array UnityStudio::get_scene_hierarchy() {
    Array root_nodes;
    std::unordered_map<int64_t, UnityGameObject> game_objects;
    std::unordered_map<int64_t, UnityTransform> transforms;

    for (const auto& sfile : loaded_files) {
        for (const auto& obj : sfile.objects) {
            uint32_t size = 0;
            int version = 0;
            const uint8_t* data = get_object_data(obj.path_id, size, version);
            if (!data) continue;

            if (obj.class_id == 1) {
                UnityGameObject go;
                if (go.parse(data, size, version)) game_objects[obj.path_id] = go;
            } else if (obj.class_id == 4) {
                UnityTransform tr;
                if (tr.parse(data, size, version)) transforms[obj.path_id] = tr;
            }
        }
    }

    for (const auto& pair : transforms) {
        int64_t tr_id = pair.first;
        const auto& tr = pair.second;

        if (tr.father_path_id == 0) {
            auto go_it = game_objects.find(tr.game_object_path_id);
            if (go_it != game_objects.end()) {
                Dictionary node;
                node["path_id"] = go_it->first;
                node["name"] = String::utf8(go_it->second.name.c_str());
                node["transform_id"] = tr_id;
                root_nodes.push_back(node);
            }
        }
    }
    return root_nodes;
}

// ⚡ الماسح الذكي (Smart Pointer Scanner) لربط الأبواب والمجسمات المتحركة ⚡
Node3D* UnityStudio::build_game_object_model(int64_t go_path_id) {
    Node3D* root_node = memnew(Node3D);

    std::unordered_map<int64_t, UnityGameObject> go_map;
    std::unordered_map<int64_t, UnityTransform> tr_map;
    std::unordered_map<int64_t, int64_t> go_to_mesh_map;
    std::unordered_set<int64_t> all_mesh_ids;

    // 1. فهرسة سريعة
    for (const auto& sfile : loaded_files) {
        for (const auto& obj : sfile.objects) {
            if (obj.class_id == 43) all_mesh_ids.insert(obj.path_id); // حصر كل المجسمات

            uint32_t size = 0;
            int version = 0;
            const uint8_t* data = get_object_data(obj.path_id, size, version);
            if (!data) continue;

            if (obj.class_id == 1) {
                UnityGameObject go;
                if (go.parse(data, size, version)) go_map[obj.path_id] = go;
            } else if (obj.class_id == 4) {
                UnityTransform tr;
                if (tr.parse(data, size, version)) tr_map[obj.path_id] = tr;
            } 
            // الماسح الذكي لـ MeshFilter و SkinnedMeshRenderer (الأبواب والشخصيات)
            else if (obj.class_id == 33 || obj.class_id == 137) { 
                UnityReader r(data, size);
                r.read_i32_le(); // game object file_id
                int64_t comp_go_id = (version >= 14) ? r.read_i64_le() : static_cast<int64_t>(r.read_i32_le());
                
                int64_t found_mesh_id = 0;
                // مسح البايتات بحثاً عن أي مؤشر يطابق مجسماً حقيقياً!
                for (size_t i = 0; i <= size - 8; i += 4) {
                    r.set_position(i);
                    r.read_i32_le(); // file id
                    int64_t ptr_id = (version >= 14) ? r.read_i64_le() : static_cast<int64_t>(r.read_i32_le());
                    if (ptr_id != 0 && all_mesh_ids.count(ptr_id)) {
                        found_mesh_id = ptr_id;
                        break;
                    }
                }
                if (comp_go_id != 0 && found_mesh_id != 0) {
                    go_to_mesh_map[comp_go_id] = found_mesh_id;
                }
            }
        }
    }

    auto root_go_it = go_map.find(go_path_id);
    if (root_go_it == go_map.end()) return root_node;

    root_node->set_name(String::utf8(root_go_it->second.name.c_str()));

    std::vector<int64_t> queue_transforms;
    if (root_go_it->second.transform_path_id != 0) {
        queue_transforms.push_back(root_go_it->second.transform_path_id);
    }

    Ref<StandardMaterial3D> pbr_mat = memnew(StandardMaterial3D);
    pbr_mat->set_albedo(Color(0.85, 0.88, 0.92));
    pbr_mat->set_metallic(0.25);
    pbr_mat->set_roughness(0.45);

    while (!queue_transforms.empty()) {
        int64_t cur_tr_id = queue_transforms.back();
        queue_transforms.pop_back();

        auto tr_it = tr_map.find(cur_tr_id);
        if (tr_it == tr_map.end()) continue;

        int64_t cur_go_id = tr_it->second.game_object_path_id;

        for (int64_t child_tr_id : tr_it->second.children_path_ids) {
            queue_transforms.push_back(child_tr_id);
        }

        auto mesh_it = go_to_mesh_map.find(cur_go_id);
        if (mesh_it != go_to_mesh_map.end()) {
            Ref<ArrayMesh> mesh_res = get_mesh(mesh_it->second);
            if (mesh_res.is_valid()) {
                MeshInstance3D* mi = memnew(MeshInstance3D);
                mi->set_mesh(mesh_res);
                mi->set_material_override(pbr_mat);
                mi->set_position(tr_it->second.local_position);
                mi->set_quaternion(tr_it->second.local_rotation);
                mi->set_scale(tr_it->second.local_scale);
                root_node->add_child(mi);
            }
        }
    }

    return root_node;
}

String UnityStudio::get_text_asset(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return "";
    UnityTextAsset text;
    if (text.parse(data, size)) return String::utf8(text.script.c_str());
    return "";
}

Ref<Image> UnityStudio::get_texture_image(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return Ref<Image>();
    UnityTexture2D tex;
    if (tex.parse(data, size, version)) return tex.create_godot_image();
    return Ref<Image>();
}

Ref<ArrayMesh> UnityStudio::get_mesh(int64_t path_id) {
    uint32_t size = 0;
    int version = 0;
    const uint8_t* data = get_object_data(path_id, size, version);
    if (!data) return Ref<ArrayMesh>();
    UnityMesh mesh;
    if (mesh.parse(data, size, version)) return mesh.create_godot_mesh();
    return Ref<ArrayMesh>();
}
