#pragma once
#include <string>
#include "glm/glm.hpp"
#include "Framework/Util.h"
struct ExportVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv = glm::vec2(0.f);
	glm::vec2 uv2 = glm::vec2(0.f);
};
struct Part
{
	int vert_ofs = 0;
	int vert_count = 0;
	int index_ofs = 0;
	int index_count = 0;
	Color32 materialAlbedo = COLOR_WHITE;
	Color32 materialEmissive = COLOR_BLACK;
	// Only used when export_to_gltf's separate_nodes=true (e.g. AgentBridge's per-MeshComponent
	// export); ignored otherwise. Falls back to "part_N" if left empty.
	std::string node_name;
};
// separate_nodes=false (default) preserves the original behavior: all parts merged into one
// shared glTF mesh/node. separate_nodes=true instead emits one glTF node (named from
// Part::node_name) per part, each with its own single-primitive mesh.
bool export_to_gltf(const std::vector<Part>& parts, const std::vector<ExportVertex>& vertices,
					const std::vector<uint32_t>& indices, const char* output_path, bool separate_nodes = false);