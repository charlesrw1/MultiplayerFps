#pragma once
#include "glm/glm.hpp"
#include "Framework/Util.h"
struct ExportVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv=glm::vec2(0.f);
    glm::vec2 uv2=glm::vec2(0.f);
};
struct Part {
    int vert_ofs = 0;
    int vert_count = 0;
    int index_ofs = 0;
    int index_count = 0;
    Color32 materialAlbedo = COLOR_WHITE;
    Color32 materialEmissive = COLOR_BLACK;
};
bool export_to_gltf(const std::vector<Part>& parts, const std::vector<ExportVertex>& vertices, const std::vector<uint32_t>& indices, const char* output_path);