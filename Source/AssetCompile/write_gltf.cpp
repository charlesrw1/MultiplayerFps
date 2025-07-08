#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"
#include <vector>
#include "glm/glm.hpp"
#include <fstream>
#include "Framework/SysPrint.h"
#include "GltfExport.h"
#include "Framework/StringUtils.h"

bool export_to_gltf(const std::vector<ExportVertex>& vertices, const std::vector<uint32_t>& indices, const char* output_path) {
    cgltf_data data = {};
    data.file_type = cgltf_file_type_gltf;
    data.asset.version = const_cast<char*>("2.0");

    std::string binpath = StringUtils::strip_extension(output_path) + ".bin";

    // Flatten vertex + index data into a single buffer
    std::vector<uint8_t> buffer_data;
    size_t vertex_offset = 0;
    size_t index_offset = 0;

    vertex_offset = buffer_data.size();
    buffer_data.resize(vertex_offset + vertices.size() * sizeof(ExportVertex));
    std::memcpy(buffer_data.data() + vertex_offset, vertices.data(), vertices.size() * sizeof(ExportVertex));

    index_offset = buffer_data.size();
    buffer_data.resize(index_offset + indices.size() * sizeof(uint32_t));
    std::memcpy(buffer_data.data() + index_offset, indices.data(), indices.size() * sizeof(uint32_t));

    // Create buffer
    cgltf_buffer buffer = {};
    buffer.size = buffer_data.size();
    std::string binFilenameOnly = binpath;
    StringUtils::get_filename(binFilenameOnly);
    binFilenameOnly += ".bin";
    buffer.uri = const_cast<char*>(binFilenameOnly.c_str());
    data.buffers = &buffer;
    data.buffers_count = 1;

    // Create buffer views
    cgltf_buffer_view views[2] = {};

    // Vertex buffer view
    views[0].buffer = &buffer;
    views[0].offset = vertex_offset;
    views[0].size = vertices.size() * sizeof(ExportVertex);
    views[0].stride = sizeof(ExportVertex);
    views[0].type = cgltf_buffer_view_type_vertices;

    // Index buffer view
    views[1].buffer = &buffer;
    views[1].offset = index_offset;
    views[1].size = indices.size() * sizeof(uint32_t);
    views[1].stride = 0;
    views[1].type = cgltf_buffer_view_type_indices;

    data.buffer_views = views;
    data.buffer_views_count = 2;

    // Create accessors
    cgltf_accessor accessors[5] = {};

    // POSITION accessor
    accessors[0].buffer_view = &views[0];
    accessors[0].component_type = cgltf_component_type_r_32f;
    accessors[0].type = cgltf_type_vec3;
    accessors[0].count = vertices.size();
    accessors[0].offset = 0; // position is first in ExportVertex
    accessors[0].stride = sizeof(ExportVertex);

    // NORMAL accessor
    accessors[1].buffer_view = &views[0];
    accessors[1].component_type = cgltf_component_type_r_32f;
    accessors[1].type = cgltf_type_vec3;
    accessors[1].count = vertices.size();
    accessors[1].offset = sizeof(glm::vec3); // normal follows position
    accessors[1].stride = sizeof(ExportVertex);

    // UV accessor
    accessors[2].buffer_view = &views[0];
    accessors[2].component_type = cgltf_component_type_r_32f;
    accessors[2].type = cgltf_type_vec2;
    accessors[2].count = vertices.size();
    accessors[2].offset = sizeof(glm::vec3)+sizeof(glm::vec3); // normal follows position
    accessors[2].stride = sizeof(ExportVertex);

    // UV2 accessor
    accessors[3].buffer_view = &views[0];
    accessors[3].component_type = cgltf_component_type_r_32f;
    accessors[3].type = cgltf_type_vec2;
    accessors[3].count = vertices.size();
    accessors[3].offset = sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2); // normal follows position
    accessors[3].stride = sizeof(ExportVertex);


    // INDEX accessor
    accessors[4].buffer_view = &views[1];
    accessors[4].component_type = cgltf_component_type_r_32u;
    accessors[4].type = cgltf_type_scalar;
    accessors[4].count = indices.size();
    accessors[4].offset = 0;

    data.accessors = accessors;
    data.accessors_count = 5;

    // Create primitive
    cgltf_primitive primitive = {};
    primitive.attributes = new cgltf_attribute[4];
    primitive.attributes[0].name = const_cast<char*>("POSITION");
    primitive.attributes[0].type = cgltf_attribute_type_position;
    primitive.attributes[0].data = &accessors[0];

    primitive.attributes[1].name = const_cast<char*>("NORMAL");
    primitive.attributes[1].type = cgltf_attribute_type_normal;
    primitive.attributes[1].data = &accessors[1];

    primitive.attributes[2].name = const_cast<char*>("TEXCOORD_0");
    primitive.attributes[2].type = cgltf_attribute_type_texcoord;
    primitive.attributes[2].data = &accessors[2];

    primitive.attributes[3].name = const_cast<char*>("TEXCOORD_1");
    primitive.attributes[3].type = cgltf_attribute_type_texcoord;
    primitive.attributes[3].data = &accessors[3];

    primitive.attributes_count = 4;
    primitive.indices = &accessors[4];
    primitive.type = cgltf_primitive_type_triangles;

    // Create mesh
    cgltf_mesh mesh = {};
    mesh.name = const_cast<char*>("Mesh");
    mesh.primitives = &primitive;
    mesh.primitives_count = 1;

    data.meshes = &mesh;
    data.meshes_count = 1;

    // Create node
    cgltf_node node = {};
    node.mesh = &mesh;

    data.nodes = &node;
    data.nodes_count = 1;

    // Create scene
    cgltf_scene scene = {};
    scene.nodes = &data.nodes;
    scene.nodes_count = 1;

    data.scene = &scene;
    data.scenes = &scene;
    data.scenes_count = 1;

    // Write .gltf file
    cgltf_options options = {};
    if (cgltf_write_file(&options, output_path, &data) != cgltf_result_success) {
        delete[] primitive.attributes;
        return false;
    }

    // Write binary buffer
    std::ofstream bin_file(binpath, std::ios::binary);
    bin_file.write(reinterpret_cast<const char*>(buffer_data.data()), buffer_data.size());
    bin_file.close();

    delete[] primitive.attributes;
    return true;
}