#ifdef EDITOR_BUILD
#include <vector>
#include <fstream>
#include <cstring>
#include "glm/glm.hpp"
#include "AssetCompile/GltfExport.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Render/Model.h"
#include "Level.h"

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

constexpr float POSITION_EPSILON = 0.01;
constexpr float NORMAL_EPSILON = 0.001;

void export_one_model(const Model& model, const char* export_path);

glm::vec3 get_normal_from_v(const ModelVertex& mv) {
    return glm::normalize(glm::vec3(double(mv.normal[0]) / INT16_MAX, double(mv.normal[1]) / INT16_MAX, double(mv.normal[2]) / INT16_MAX));
}
glm::vec2 get_uv_from_v(const ModelVertex& mv) {
    return glm::normalize(glm::vec3(double(mv.normal[0]) / INT16_MAX, double(mv.normal[1]) / INT16_MAX, double(mv.normal[2]) / INT16_MAX));
}
glm::vec2 get_uv2_from_v(const ModelVertex& mv) {
    int uvX = int(mv.color[0]) | (int(mv.color[1]) << 8);
    int uvY = int(mv.color[2]) | (int(mv.color[3]) << 8);
    return glm::vec2(double(uvX) / UINT16_MAX, double(uvY) / UINT16_MAX);

}
#include "Framework/StringUtils.h"

std::string write_export_string(const Model* model) {

    string export_path = StringUtils::alphanumeric_hash(model->get_name())+".glb";

    std::string out = "[remap]\n";
    out +=
        "importer = \"scene\"\n"
        "importer_version = 1\n"
        "type = \"PackedScene\"\n";
    out += "[deps]\n";
    out += "source_file = \"" + export_path + "\"\n";
    out += "[params]\n";
    auto size = model->get_lightmap_size();
    out += "meshes/lightmap_size_hint = Vector2i(" + std::to_string(size.x)+"," + std::to_string(size.y) + ")\n";

    return out;
}
std::string write_transform3d(glm::mat4 transformIn) {
    glm::mat3 transform = glm::transpose(glm::mat3(transformIn));

    return "Transform3D(" + std::string(string_format("%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
        transform[0].x, transform[0].y, transform[0].z,
        transform[1].x, transform[1].y, transform[1].z,
        transform[2].x, transform[2].y, transform[2].z,
        transformIn[3].x, transformIn[3].y, transformIn[3].z)) + ")";
}
#include "Framework/Files.h"
void export_godot_scene(const std::string& base_export_path) {
    auto& objs = eng->get_level()->get_all_objects();
    const bool export_only_visible = false;

    std::unordered_set<const Model*> models_to_export;
    std::string out_scene_file;
    out_scene_file += "[node name=\"Node3D\" type=\"Node3D\"]\n";
    for (auto obj : objs) {
        if (auto as_ent = obj->cast_to<Entity>()) {
            auto mesh = as_ent->get_component<MeshComponent>();
            if (export_only_visible && !as_ent->get_hidden_in_editor())
                continue;
            if (!mesh)
                continue;
            if (mesh->dont_serialize_or_edit)
                continue;
            if (!mesh->get_model())
                continue;
            const Model* model = mesh->get_model();
            if (model->get_num_lods() > 0&& model->has_lightmap_coords()) {

                models_to_export.insert(model);
                auto& transform = as_ent->get_ws_transform();

                /*
                
                [node name="floor" parent="floor/floor5/floor3/floor2" instance=ExtResource("4_r3fl7")]
                transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, 2, 0, 0)
                */
                std::string model_inst_id = StringUtils::alphanumeric_hash(model->get_name());
                out_scene_file += "[node name = \"mesh_" + std::to_string(mesh->unique_file_id) + "\" parent=\".\" instance=ExtResource(\"" + model_inst_id + "\")]\n";
                out_scene_file += "transform = " + write_transform3d(transform) + "\n";
            }
        }
    }
    for (auto obj : objs) {
        if (auto as_ent = obj->cast_to<Entity>()) {
            if (export_only_visible && !as_ent->get_hidden_in_editor())
                continue;
            auto& transform = as_ent->get_ws_transform();

            auto write_color = [](Color32 c) -> std::string {
                auto vec = color32_to_vec4(c);
                return string_format("Color(%f,%f,%f,1)", vec.x, vec.y, vec.z);
            };

            if (auto point = as_ent->get_component<PointLightComponent>()) {
                out_scene_file += "[node name=\"light" + std::to_string(point->unique_file_id);
                out_scene_file +=  "\" type=\"OmniLight3D\" parent=\".\"]\n";
                out_scene_file += "transform = " + write_transform3d(transform) + "\n";
                out_scene_file += "light_energy = " + std::to_string(point->intensity) + "\n";
                out_scene_file += "light_color = " + write_color(point->color) + "\n";
                out_scene_file += "omni_range = " + std::to_string(point->radius) + "\n";
                out_scene_file += "shadow_enabled = true\n";
            }
            else if (auto spot = as_ent->get_component<SpotLightComponent>()) {
                out_scene_file += "[node name=\"light" + std::to_string(spot->unique_file_id);
                out_scene_file += "\" type=\"SpotLight3D\" parent=\".\"]\n";
                out_scene_file += "transform = " + write_transform3d(transform) + "\n";
                out_scene_file += "light_energy = " + std::to_string(spot->intensity) + "\n";
                out_scene_file += "light_color = " + write_color(spot->color) + "\n";
                out_scene_file += "spot_angle = " + std::to_string(spot->cone_angle) + "\n";
                out_scene_file += "spot_range = " + std::to_string(spot->radius) + "\n";
                out_scene_file += "shadow_enabled = true\n";

            }
            else if (auto sun = as_ent->get_component<SunLightComponent>()) {
                out_scene_file += "[node name=\"light" + std::to_string(sun->unique_file_id);
                out_scene_file += "\" type=\"DirectionalLight3D\" parent=\".\"]\n";
                out_scene_file += "transform = " + write_transform3d(transform) + "\n";
                out_scene_file += "light_energy = " + std::to_string(sun->intensity) + "\n";
                out_scene_file += "light_color = " + write_color(sun->color) + "\n";
                out_scene_file += "shadow_enabled = true\n";
            }

        }
        /*
        [node name="OmniLight3D3" type="OmniLight3D" parent="."]
transform = Transform3D(-0.430322, 0, 0.902676, 0, 1, 0, -0.902676, 0, -0.430322, -8.1315, 1.09958, 2.33485)
light_color = Color(0.972773, 0.662063, 0.800588, 1)
shadow_enabled = true
        */
    }

    std::string header;
    header += "[gd_scene format=4]\n";
    for (auto model : models_to_export) {
        /*[ext_resource type="PackedScene" uid="uid://bd50f2ogsm3ka" path="res://boat.tscn" id="3_jka67"]*/      
        std::string model_inst_id = StringUtils::alphanumeric_hash(model->get_name());
        header += "[ext_resource type=\"PackedScene\" path=\"" + model_inst_id + ".glb\" id=\"" + model_inst_id + "\"]\n";
    }
    std::string outFile = header + out_scene_file;

    {
        std::ofstream outfile(base_export_path + "scene_file.tscn");
        if (outfile) {
            outfile.write(outFile.data(), outFile.size());
        }
        else {
            assert(0);
        }
        for (auto model : models_to_export) {
            std::string model_out_path = StringUtils::alphanumeric_hash(model->get_name()) + ".glb";
            std::string exp_str = write_export_string(model);
            {
                auto outModelPath = (base_export_path + model_out_path);
                auto godotModelFile = FileSys::open_read(outModelPath.c_str(), FileSys::FULL_SYSTEM);
                if (godotModelFile) {
                    auto theGameModel = FileSys::open_read_game(model->get_name());
                    if (theGameModel && theGameModel->get_timestamp() <= godotModelFile->get_timestamp()) {
                        sys_print(Debug, "export_godot_scene: skipping model export %s\n", model->get_name().c_str());
                        continue;
                    }
                    godotModelFile->close();
                }
                


                std::ofstream outfile((base_export_path + model_out_path + ".import"));
                assert(outfile);
                outfile.write(exp_str.data(), exp_str.size());
            }
            {
                export_one_model(*model, (base_export_path + model_out_path).c_str());
            }
        }
    }
}
inline Color32 vec_to_color32(glm::vec4 v) {
    return Color32(v.r * 255.0, v.g * 255.0, v.b * 255.0, v.a);
}
inline Color32 srgb_to_linear_color32(Color32 c) {
    auto vec = color32_to_vec4(c);
    auto linear = colorvec_srgb_to_linear(vec);
    return vec_to_color32(linear);
}

#include "Render/MaterialPublic.h"
#include "Render/MaterialLocal.h"
#include "Render/Texture.h"
Color32 get_color_of_material_for_export(const MaterialInstance* m) {
    if (!m) return COLOR_PINK;
    auto master = m->get_master_material();
    auto impl = m->impl.get();
    if (!master) return COLOR_PINK;
    if (!master->self) return COLOR_PINK;
    if (master->self->get_name() == "eng/fallback.mm") {
        auto p = impl->find_parameter(StringName("BaseColor"));
        if (!p||!p->tex) return COLOR_PINK;
        return p->tex->simplifiedColor;
    }
    if (master->self->get_name() == "defaultPBR.mm") {
        auto p = impl->find_parameter(StringName("Albedo"));
        if (!p || !p->tex) return COLOR_PINK;
        auto texColor = color32_to_vec4(p->tex->simplifiedColor);
        auto c = impl->find_parameter(StringName("colorMult"));
        if (!c || c->type != MatParamType::Vector) return COLOR_PINK;
        auto multColor = color32_to_vec4(c->color32);
        return vec_to_color32(texColor * multColor);
    }
    return COLOR_PINK;
}

void export_one_model(const Model& model, const char* export_path) {
    std::vector<ExportVertex> verticies;
    std::vector<uint32_t> indicies;
    std::vector<Part> parts;
    auto& lod = model.get_lod(0);
    const RawMeshData* meshData = model.get_raw_mesh_data();
    for (int partI = lod.part_ofs; partI < lod.part_ofs + lod.part_count; partI++) {
        auto& part = model.get_part(partI);
        Part p;
        const int new_offset = verticies.size();
        const int new_index_start = indicies.size();

        for (int v = 0; v < part.vertex_count; v++) {
            auto& vertex = meshData->get_vertex_at_index(part.base_vertex + v);
            glm::vec3 pos = vertex.pos;
            pos = glm::vec4(pos, 1.0);
            ExportVertex expV{};
            expV.position = pos;
            expV.normal = get_normal_from_v(vertex);
            expV.uv = vertex.uv;
            expV.uv2 = get_uv2_from_v(vertex);
            verticies.push_back(expV);
        }
        for (int i = 0; i < part.element_count; i++) {
            const int start = part.element_offset / sizeof(int16_t);
            const uint16_t index = meshData->get_index_at_index(start + i);
            const int new_index = ((int)index) + new_offset;
            const int new_new_index = new_index - new_offset;
            indicies.push_back(new_new_index);
        }

        p.index_count = indicies.size() - new_index_start;
        p.index_ofs = new_index_start;
        p.vert_count = verticies.size() - new_offset;
        p.vert_ofs = new_offset;
        p.materialAlbedo = srgb_to_linear_color32(get_color_of_material_for_export(model.get_material(part.material_idx)));
        parts.push_back(p);
    }

    export_to_gltf(parts, verticies, indicies, export_path);
}

void export_level_scene() {
    auto& objs = eng->get_level()->get_all_objects();
    std::vector<ExportVertex> verticies;
    std::vector<uint32_t> indicies;
    const bool export_only_visible = false;
    for (auto obj : objs) {
        if (auto as_ent = obj->cast_to<Entity>()) {
            auto mesh = as_ent->get_component<MeshComponent>();
            if (export_only_visible && !as_ent->get_hidden_in_editor())
                continue;

            if (mesh && !mesh->dont_serialize_or_edit && mesh->get_model() && mesh->get_model()->get_num_lods() > 0) {
                const Model* model = mesh->get_model();
                auto& transform = as_ent->get_ws_transform();
                auto& lod = model->get_lod(0);
                const RawMeshData* meshData = model->get_raw_mesh_data();
                for (int partI = lod.part_ofs; partI < lod.part_ofs + lod.part_count; partI++) {
                    auto& part = model->get_part(partI);
                    const int new_offset = verticies.size();
                    const int new_index_start = indicies.size();
                    for (int v = 0; v < part.vertex_count; v++) {
                        auto& vertex = meshData->get_vertex_at_index(part.base_vertex + v);
                        glm::vec3 pos = vertex.pos;
                        pos = transform * glm::vec4(pos,1.0);
                        ExportVertex expV{};
                        expV.position = pos;
                        expV.normal = glm::mat3(transform)*get_normal_from_v(vertex);
                        expV.uv = vertex.uv;
                        verticies.push_back(expV);
                    }
                    for (int i = 0; i < part.element_count; i++) {
                        const int start = part.element_offset / sizeof(int16_t);
                        const uint16_t index = meshData->get_index_at_index(start + i);
                        const int new_index = ((int)index) + new_offset;
                        indicies.push_back(new_index);
                    }
                }

            }
        }
    }
    Part p;
    p.index_count = indicies.size();
    p.index_ofs = 0;
    p.vert_count = verticies.size();
    p.vert_ofs = 0;
    export_to_gltf({ p },verticies, indicies, "LEVEL_OUT.glb");
}
#endif