#ifndef SERIALIZED_FILE_H
#define SERIALIZED_FILE_H

#include "unity_reader.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

struct ObjectInfo {
    int64_t path_id;
    uint64_t byte_start;
    uint32_t byte_size;
    int32_t type_id;
    int32_t class_id;
};

// Unity Class IDs
enum UnityClassID {
    ClassID_GameObject = 1,
    ClassID_Transform = 4,
    ClassID_Material = 21,
    ClassID_MeshRenderer = 23,
    ClassID_Texture2D = 28,
    ClassID_MeshFilter = 33,
    ClassID_Mesh = 43,
    ClassID_SkinnedMeshRenderer = 137
};

class SerializedFile {
public:
    uint32_t metadata_size = 0;
    uint64_t file_size = 0;
    uint32_t version = 0;
    uint64_t data_offset = 0;
    uint8_t endianess = 0;
    std::string unity_version;

    std::vector<ObjectInfo> objects;
    std::unordered_map<int64_t, size_t> path_id_to_index;

    bool parse(const uint8_t* raw_data, size_t raw_size);
};

#endif
