#pragma once
#include "glm/glm.hpp"
struct ExportVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv=glm::vec2(0.f);
    glm::vec2 uv2=glm::vec2(0.f);
};
bool export_to_gltf(const std::vector<ExportVertex>& vertices, const std::vector<uint32_t>& indices, const char* output_path);