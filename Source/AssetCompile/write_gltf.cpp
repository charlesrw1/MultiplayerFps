#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"
#include <vector>
#include "glm/glm.hpp"
#include <fstream>
#include "Framework/SysPrint.h"
#include "GltfExport.h"
#include "Framework/StringUtils.h"

bool export_to_gltf(const std::vector<Part>& parts, const std::vector<ExportVertex>& vertices, const std::vector<uint32_t>& indices, const char* output_path) {
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

    cgltf_accessor* accessors = new cgltf_accessor[5 * parts.size()];
    memset(accessors, 0, sizeof(cgltf_accessor) * 5 * parts.size());

    cgltf_primitive* primitives = new cgltf_primitive[parts.size()];
    memset(primitives, 0, sizeof(cgltf_primitive) * parts.size());

    cgltf_attribute* attributes = new cgltf_attribute[4 * parts.size()];
    memset(attributes, 0, sizeof(cgltf_attribute) * 4 * parts.size());

    cgltf_material* materials = new cgltf_material[parts.size()];
    memset(materials, 0, sizeof(cgltf_material) * parts.size());

    data.accessors = accessors;
    data.accessors_count = 5 * parts.size();
    data.materials = materials;
    data.materials_count = parts.size();

    for (int i = 0; i < parts.size(); i++) {
        auto& part = parts[i];
        // Create accessors
        //cgltf_accessor accessors[5] = {};
        int a_ofs = i * 5;
        const int vert_ofs = part.vert_ofs * sizeof(ExportVertex);
        const int index_ofs = part.index_ofs * sizeof(uint32_t);
        // POSITION accessor
        accessors[a_ofs].buffer_view = &views[0];
        accessors[a_ofs].component_type = cgltf_component_type_r_32f;
        accessors[a_ofs].type = cgltf_type_vec3;
        accessors[a_ofs].count = part.vert_count;// vertices.size();
        accessors[a_ofs].offset = vert_ofs; // position is first in ExportVertex
        accessors[a_ofs].stride = sizeof(ExportVertex);

        // NORMAL accessor
        accessors[a_ofs+1].buffer_view = &views[0];
        accessors[a_ofs+1].component_type = cgltf_component_type_r_32f;
        accessors[a_ofs+1].type = cgltf_type_vec3;
        accessors[a_ofs+1].count = part.vert_count;
        accessors[a_ofs+1].offset = vert_ofs+sizeof(glm::vec3); // normal follows position
        accessors[a_ofs+1].stride = sizeof(ExportVertex);

        // UV accessor
        accessors[a_ofs+2].buffer_view = &views[0];
        accessors[a_ofs+2].component_type = cgltf_component_type_r_32f;
        accessors[a_ofs+2].type = cgltf_type_vec2;
        accessors[a_ofs+2].count = part.vert_count;
        accessors[a_ofs+2].offset = vert_ofs+sizeof(glm::vec3) + sizeof(glm::vec3); // normal follows position
        accessors[a_ofs+2].stride = sizeof(ExportVertex);

        // UV2 accessor
        accessors[a_ofs+3].buffer_view = &views[0];
        accessors[a_ofs+3].component_type = cgltf_component_type_r_32f;
        accessors[a_ofs+3].type = cgltf_type_vec2;
        accessors[a_ofs+3].count = part.vert_count;
        accessors[a_ofs+3].offset = vert_ofs + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2); // normal follows position
        accessors[a_ofs+3].stride = sizeof(ExportVertex);


        // INDEX accessor
        accessors[a_ofs+4].buffer_view = &views[1];
        accessors[a_ofs+4].component_type = cgltf_component_type_r_32u;
        accessors[a_ofs+4].type = cgltf_type_scalar;
        accessors[a_ofs+4].count = part.index_count;
        accessors[a_ofs+4].offset = index_ofs;


        // Create primitive
        cgltf_primitive* primitive = &primitives[i];

        primitive->material = &materials[i];
        auto& mat = materials[i];
        mat.name = const_cast<char*>("Material");
        mat.has_pbr_metallic_roughness = true;
        mat.pbr_metallic_roughness.base_color_factor[0] = part.materialAlbedo.r/255.0;
        mat.pbr_metallic_roughness.base_color_factor[1] =  part.materialAlbedo.g/255.0;
        mat.pbr_metallic_roughness.base_color_factor[2] =  part.materialAlbedo.b/255.0;
        mat.pbr_metallic_roughness.base_color_factor[3] = 1.0f;
        mat.pbr_metallic_roughness.metallic_factor = 0.0;
        mat.pbr_metallic_roughness.roughness_factor = 1.0;



        primitive->attributes = &attributes[i * 4];
        primitive->attributes[0].name = const_cast<char*>("POSITION");
        primitive->attributes[0].type = cgltf_attribute_type_position;
        primitive->attributes[0].data = &accessors[a_ofs+0];

        primitive->attributes[1].name = const_cast<char*>("NORMAL");
        primitive->attributes[1].type = cgltf_attribute_type_normal;
        primitive->attributes[1].data = &accessors[a_ofs+1];

        primitive->attributes[2].name = const_cast<char*>("TEXCOORD_0");
        primitive->attributes[2].type = cgltf_attribute_type_texcoord;
        primitive->attributes[2].data = &accessors[a_ofs+2];

        primitive->attributes[3].name = const_cast<char*>("TEXCOORD_1");
        primitive->attributes[3].type = cgltf_attribute_type_texcoord;
        primitive->attributes[3].data = &accessors[a_ofs+3];

        primitive->attributes_count = 4;
        primitive->indices = &accessors[a_ofs+4];
        primitive->type = cgltf_primitive_type_triangles;
    }

    // Create mesh
    cgltf_mesh mesh = {};
    mesh.name = const_cast<char*>("Mesh");
    mesh.primitives = primitives;
    mesh.primitives_count = parts.size();

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
        delete[] attributes;
        delete[] primitives;
        delete[] accessors;
        delete[] materials;
        return false;
    }

    // Write binary buffer
    std::ofstream bin_file(binpath, std::ios::binary);
    bin_file.write(reinterpret_cast<const char*>(buffer_data.data()), buffer_data.size());
    bin_file.close();

    delete[] attributes;
    delete[] primitives;
    delete[] accessors;
    delete[] materials;
    return true;
}