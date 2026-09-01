#ifndef UNITY_SCENE_H
#define UNITY_SCENE_H

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

struct UnityVector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct UnityVector4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct SceneNode {
    int64_t game_object_path_id = 0;
    int64_t transform_path_id = 0;
    int64_t mesh_path_id = 0;
    std::string name;

    // الإحداثيات المحلية
    UnityVector3 local_position;
    UnityVector4 local_rotation; // Quaternion
    UnityVector3 local_scale;

    int64_t parent_transform_path_id = 0;
    std::vector<std::shared_ptr<SceneNode>> children;
};

class UnitySceneGraph {
public:
    std::vector<std::shared_ptr<SceneNode>> root_nodes;

    // تحويل إحداثيات Unity إلى نظام Right-Handed القياسي
    static UnityVector3 convert_position(const UnityVector3& pos) {
        return { pos.x, pos.y, -pos.z };
    }

    static UnityVector4 convert_rotation(const UnityVector4& q) {
        return { -q.x, -q.y, q.z, q.w };
    }
};

#endif
