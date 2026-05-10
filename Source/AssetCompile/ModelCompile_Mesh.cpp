#ifdef EDITOR_BUILD

#include "ModelCompilierLocal.h"
#include "Animation/SkeletonData.h"
#include "Framework/DictParser.h"
#include "Compiliers.h"
#include "Render/Model.h"
#include "cgltf.h"
#define USE_CGLTF
#include <unordered_set>
#include "Framework/Files.h"
#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

#include "Animation/AnimationUtil.h"

#include "Framework/BinaryReadWrite.h"
#include "Physics/Physics2.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>

#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"

#include <physx/cooking/PxCooking.h>

#include <fstream>

#include <meshoptimizer.h>

#define CAST_TO_AND_INDEX(index, type, buffer) ((type*)(buffer))[index]
class FormatConverter
{
public:
	static uint32_t convert_integer(const uint8_t* input_buf, cgltf_component_type type) {
		assert(type != cgltf_component_type_r_32f);
		if (type == cgltf_component_type_r_8)
			return CAST_TO_AND_INDEX(0, int8_t, input_buf);
		else if (type == cgltf_component_type_r_8u)
			return CAST_TO_AND_INDEX(0, uint8_t, input_buf);
		else if (type == cgltf_component_type_r_16)
			return CAST_TO_AND_INDEX(0, int16_t, input_buf);
		else if (type == cgltf_component_type_r_16u)
			return CAST_TO_AND_INDEX(0, uint16_t, input_buf);
		else if (type == cgltf_component_type_r_32u)
			return CAST_TO_AND_INDEX(0, uint32_t, input_buf);
		assert(0);
		return 0;
	}

	static glm::vec4 convert_to_floatvec(const uint8_t* input_buf, cgltf_component_type type, cgltf_type count,
										 bool normalized, float default_ = 0.0) {
		assert(count < cgltf_type_mat2);
		assert(type == cgltf_component_type_r_32f || normalized);
		glm::vec4 out_vec = glm::vec4(default_);

		if (type == cgltf_component_type_r_32f) {
			float* input_buf_f = (float*)input_buf;
			for (int i = 0; i < count; i++) {
				out_vec[i] = input_buf_f[i];
			}
		} else if (normalized) {
			for (int i = 0; i < count; i++) {
				if (type == cgltf_component_type_r_8u) {
					int normalized = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
					out_vec[i] = normalized / 255.0;
				} else if (type == cgltf_component_type_r_16u) {
					int normalized = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
					out_vec[i] = normalized / (double)UINT16_MAX;
				} else
					assert(0);
			}
		}
		return out_vec;
	}
	static glm::ivec4 convert_to_intvec(const uint8_t* input_buf, cgltf_component_type type, cgltf_type count,
										bool normalized) {
		assert(type != cgltf_component_type_r_32f && !normalized);
		assert(count < cgltf_type_mat2);
		glm::vec4 input;
		for (int i = 0; i < count; i++) {
			if (type == cgltf_component_type_r_8)
				input[i] = CAST_TO_AND_INDEX(i, int8_t, input_buf);
			else if (type == cgltf_component_type_r_8u)
				input[i] = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
			else if (type == cgltf_component_type_r_16)
				input[i] = CAST_TO_AND_INDEX(i, int16_t, input_buf);
			else if (type == cgltf_component_type_r_16u)
				input[i] = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
			else if (type == cgltf_component_type_r_32u)
				input[i] = CAST_TO_AND_INDEX(i, uint32_t, input_buf);
			else
				assert(0);
		}
		return input;
	}
};
#undef CAST_TO_AND_INDEX

template <typename FUNCTOR, typename T>
void convert_format_verts(FUNCTOR&& f, size_t start, std::vector<T>& verts, cgltf_accessor* ac) {
	uint8_t* buffer = (uint8_t*)ac->buffer_view->buffer->data + (ac->buffer_view->offset + ac->offset);
	assert(verts.size() == start + ac->count);
	assert(ac->stride != 0);
	for (int i = 0; i < ac->count; i++) {
		uint8_t* ptr = buffer + i * ac->stride;
		f(verts[start + i], ptr, ac->component_type, ac->type, ac->normalized);
	}
}

void append_a_found_mesh_node(const ModelDefData& def, ModelCompileData& mcd, cgltf_node* node,
							  const glm::mat4& transform, bool is_collision_node, ShapeType_e shape) {
	ASSERT(node->mesh);

	cgltf_mesh* mesh = node->mesh;

	std::string node_name = node->name;

	int lod = 0;
	if (node_name.find("LOD0_") == 0)
		lod = 0;
	else if (node_name.find("LOD1_") == 0)
		lod = 1;
	else if (node_name.find("LOD2_") == 0)
		lod = 2;
	else if (node_name.find("LOD3_") == 0)
		lod = 3;
	else if (node_name.find("LOD4_") == 0)
		lod = 4;

	if (mcd.lod_where.size() <= lod)
		mcd.lod_where.resize(lod + 1);

	for (int i = 0; i < mesh->primitives_count; i++) {
		const cgltf_primitive& prim = mesh->primitives[i];

		Submesh part;
		Bounds bounds;

		cgltf_accessor* indicies_accessor = prim.indices;

		part.base_vertex = mcd.verticies.size();
		part.element_offset = mcd.indicies.size() * sizeof(uint32_t);
		part.element_count = indicies_accessor->count;

		const size_t index_start = mcd.indicies.size();
		const int index_count = indicies_accessor->count;
		mcd.indicies.resize(index_start + index_count);

		convert_format_verts([](uint32_t& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type,
								bool normalized) { v = FormatConverter::convert_integer(ptr, ct); },
							 index_start, mcd.indicies, indicies_accessor);

		part.material_idx = -1;
		if (prim.material)
			part.material_idx = cgltf_material_index(mcd.gltf_file, prim.material);

		assert(prim.attributes_count >= 1 && prim.attributes[0].type == cgltf_attribute_type_position);
		const int vert_count = prim.attributes[0].data->count;
		const size_t vert_start = part.base_vertex;
		part.vertex_count = vert_count;
		mcd.verticies.resize(vert_start + vert_count);

		int attrib_mask = 0;

		for (int at_index = 0; at_index < prim.attributes_count; at_index++) {
			cgltf_attribute& attribute = prim.attributes[at_index];
			cgltf_accessor& accessor = *attribute.data;
			int byte_stride = accessor.stride;

			int location = -1;
			if (strcmp(attribute.name, "POSITION") == 0) {

				location = CMA_POSITION;

				bounds = bounds_union(bounds, glm::vec3(accessor.min[0], accessor.min[1], accessor.min[2]));
				bounds = bounds_union(bounds, glm::vec3(accessor.max[0], accessor.max[1], accessor.max[2]));

				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.position = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "TEXCOORD_0") == 0) {
				location = CMA_UV;
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.uv = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "TEXCOORD_1") == 0) {
				location = CMA_UV2;
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.uv2 = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "NORMAL") == 0) {
				location = CMA_NORMAL;
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.normal = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "JOINTS_0") == 0) {
				location = CMA_BONEINDEX;
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.bone_index = FormatConverter::convert_to_intvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "WEIGHTS_0") == 0) {
				location = CMA_BONEWEIGHT;
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.bone_weight = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
			} else if (strcmp(attribute.name, "COLOR_0") == 0) {
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.color = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized, 1.f);
					},
					vert_start, mcd.verticies, &accessor);
				location = CMA_COLOR;
			} else if (strcmp(attribute.name, "TANGENT") == 0) {
				convert_format_verts(
					[](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized) {
						v.tangent = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					},
					vert_start, mcd.verticies, &accessor);
				location = CMA_TANGENT;
			} else if (strcmp(attribute.name, "COLOR_1") == 0) {
				sys_print(Warning, "mesh has COLOR_1 attribute, ignoring it\n");
			}

			if (location == -1)
				continue;

			attrib_mask |= (1 << location);
		}

		if (!is_collision_node && !(attrib_mask & (1 << CMA_TANGENT))) {
			sys_print(Warning, "mesh not exported with tangents, computing them now(rexporting is better)\n");

			if (!(attrib_mask & CMA_UV)) {
				sys_print(Warning, "mesh not exported with uvs, fix this!\n");
			}

			for (int i = 0; i < index_count; i += 3) {
				int full_index = i + index_start;
				auto& v0 = mcd.verticies.at(vert_start + mcd.indicies.at(full_index));
				auto& v1 = mcd.verticies.at(vert_start + mcd.indicies.at(full_index + 1));
				auto& v2 = mcd.verticies.at(vert_start + mcd.indicies.at(full_index + 2));
				glm::vec3 edge_1 = v1.position - v0.position;
				glm::vec3 edge_2 = v2.position - v0.position;
				glm::vec2 deltauv_1 = v1.uv - v0.uv;
				glm::vec2 deltauv_2 = v2.uv - v0.uv;
				float denom = (deltauv_1.x * deltauv_2.y - deltauv_2.x * deltauv_1.y);

				glm::vec3 output_tangent = glm::vec3(0.f);
				float output_handedness = 1.0;
				if (glm::abs(glm::dot(v0.normal, glm::vec3(0, 1, 0))) < 0.9999) {
					output_tangent = glm::normalize(glm::cross(v0.normal, glm::vec3(0, 1, 0)));
				} else {
					output_tangent = glm::normalize(glm::cross(v0.normal, glm::vec3(1, 0, 0)));
				}

				if (glm::abs(denom) > 0.0000001f) {
					float f = 1.0f / denom;
					glm::vec3 tangent = f * (deltauv_2.y * edge_1 - deltauv_1.y * edge_2);

					glm::vec3 bitangent = f * (-deltauv_2.x * edge_1 + deltauv_1.x * edge_2);
					bitangent = glm::normalize(bitangent);
					float len = glm::length(tangent);
					if (len > 0.000001f)
						output_tangent = tangent / len;

					output_handedness = (glm::dot(glm::cross(v0.normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
				}
				v0.tangent = v1.tangent = v2.tangent = glm::vec4(output_tangent, output_handedness);
			}
		}

		LODMesh m;
		m.submesh = part;
		m.bounds = bounds;
		m.ref.globaltransform = transform;
		m.ref.index = cgltf_node_index(mcd.gltf_file, node);
		m.attribute_mask = attrib_mask;
		m.mark_for_delete = false;
		m.shape_type = shape;

		if (is_collision_node)
			mcd.physics_nodes.push_back(m);
		else
			mcd.lod_where[lod].mesh_nodes.push_back(m);
	}
}

static void traverse_model_nodes(const ModelDefData& def, ModelCompileData& mcd, const cgltf_skin* using_skin,
								 cgltf_node* node, glm::mat4 transform) {
	glm::mat4 this_transform = transform * get_node_transform(node);

	std::string node_name = node->name;
	ShapeType_e shape = ShapeType_e::None;
	if (node_name.find("BOX_") == 0)
		shape = ShapeType_e::Box;
	else if (node_name.find("CAP_") == 0)
		shape = ShapeType_e::Capsule;
	else if (node_name.find("SPH_") == 0)
		shape = ShapeType_e::Sphere;
	else if (node_name.find("CVX_") == 0)
		shape = ShapeType_e::ConvexShape;
	else if (node_name.find("TRI_") == 0)
		shape = ShapeType_e::MeshShape;

	bool has_mesh = node->mesh;
	if (shape != ShapeType_e::None) {
		if (has_mesh)
			sys_print(Debug, "found collision item %s\n", node_name.c_str());
		else
			sys_print(Warning, "node has collision name but no mesh %s\n", node_name.c_str());
	} else if (using_skin != node->skin && has_mesh) {
		has_mesh = false;
		sys_print(Warning, "this model is skinned but found a mesh node that isn't parented to it, skipping it\n");
	}

	if (has_mesh) {
		append_a_found_mesh_node(def, mcd, node, this_transform, shape != ShapeType_e::None, shape);
	}

	for (int i = 0; i < node->children_count; i++)
		traverse_model_nodes(def, mcd, using_skin, node->children[i], this_transform);
}

static void mark_used_bones_R(int this_index, const SkeletonCompileData* scd, std::vector<bool>& bone_refed) {
	int parent = scd->get_bone_parent(this_index);
	if (bone_refed[this_index] && parent != -1) {
		bone_refed[parent] = true;
		mark_used_bones_R(parent, scd, bone_refed);
	}
}

ConfigVar modcompile_print_pruned_bones("modcompile.print_pruned_bones", "0", CVAR_BOOL,
										"print bones that were pruned when a model compiles");
ConfigVar modcompile_disable_pruning_bones("modcompile_disable_pruning_bones", "0", CVAR_BOOL, "");

ProcessMeshOutput ModelCompileHelper::process_mesh(ModelCompileData& mcd, const SkeletonCompileData* scd,
												   const ModelDefData& def) {
	std::vector<bool> material_is_used(mcd.gltf_file->materials_count + 1, false);

	std::vector<bool> bone_is_referenced;

	if (scd) {
		const bool default_val = (modcompile_disable_pruning_bones.get_bool());
		bone_is_referenced.resize(scd->get_num_bones(), default_val);

		for (int i = 0; i < def.keepbones.size(); i++) {

			int index = scd->get_bone_for_name(def.keepbones[i]);
			if (index == -1) {
				sys_print(Error, "keepbone does not name a skeleton bone %s\n", def.keepbones[i].c_str());
			} else
				bone_is_referenced[index] = true;
		}
	}

	for (int i = 0; i < mcd.lod_where.size(); i++) {
		auto& lod = mcd.lod_where[i];

		vector<LODMesh> copied_output = lod.mesh_nodes;
		const int NUM_NODES_PRE_ADD = lod.mesh_nodes.size();
		for (int MESH_NODE_IDX = 0; MESH_NODE_IDX < NUM_NODES_PRE_ADD; MESH_NODE_IDX++) {
			auto& mesh = lod.mesh_nodes[MESH_NODE_IDX];

			if (!mesh.has_bones() && scd != nullptr) {
				sys_print(Warning, "nobone mesh made it past filters?\n");
				mesh.mark_for_delete = true;
				continue;
			}

			if (!mesh.has_normals()) {
				sys_print(Warning, "mesh was exported without normals, skipping it...\n");
				mesh.mark_for_delete = true;
				continue;
			}

			if (mesh.submesh.material_idx != -1)
				material_is_used.at(mesh.submesh.material_idx) = true;
			else
				material_is_used.at(material_is_used.size() - 1) = true;

			const uint32_t MAX_VERTICIES_PER_SUBMESH = UINT16_MAX - 1000;
			if (mesh.submesh.vertex_count >= MAX_VERTICIES_PER_SUBMESH) {

				uint32_t added_verticies = 0;
				uint32_t added_indicies = 0;
				const int index_offset = mesh.submesh.element_offset / sizeof(uint32_t);
				std::unordered_map<uint32_t, uint32_t> new_vertex_to_old_vertex;
				Submesh NEW_SUBMESH;
				NEW_SUBMESH.element_offset = mcd.indicies.size() * sizeof(uint32_t);
				NEW_SUBMESH.base_vertex = mcd.verticies.size();
				NEW_SUBMESH.material_idx = mesh.submesh.material_idx;
				auto find_or_append = [&](uint32_t OLD_INDEX) -> uint32_t {
					auto find = new_vertex_to_old_vertex.find(OLD_INDEX);
					if (find != new_vertex_to_old_vertex.end())
						return find->second;
					new_vertex_to_old_vertex.insert(
						{OLD_INDEX, added_verticies});
					added_verticies++;
					mcd.verticies.push_back(
						mcd.verticies.at(mesh.submesh.base_vertex + OLD_INDEX));
					return added_verticies - 1;
				};
				for (int INDEX = 0; INDEX < mesh.submesh.element_count; INDEX += 3) {
					uint32_t i0 = mcd.indicies[index_offset + INDEX];
					uint32_t i1 = mcd.indicies[index_offset + INDEX + 1];
					uint32_t i2 = mcd.indicies[index_offset + INDEX + 2];
					uint32_t newIndex0 = find_or_append(i0);
					uint32_t newIndex1 = find_or_append(i1);
					uint32_t newIndex2 = find_or_append(i2);
					added_indicies += 3;
					mcd.indicies.push_back(newIndex0);
					mcd.indicies.push_back(newIndex1);
					mcd.indicies.push_back(newIndex2);

					if (added_verticies >= MAX_VERTICIES_PER_SUBMESH) {
						NEW_SUBMESH.element_count = added_indicies;
						NEW_SUBMESH.vertex_count = added_verticies;

						LODMesh copiedLodMesh = mesh;
						copiedLodMesh.submesh = NEW_SUBMESH;
						copiedLodMesh.mark_for_delete = false;
						copied_output.push_back(copiedLodMesh);

						NEW_SUBMESH.base_vertex = mcd.verticies.size();
						NEW_SUBMESH.element_count = 0;
						NEW_SUBMESH.vertex_count = 0;
						NEW_SUBMESH.element_offset = mcd.indicies.size() * sizeof(uint32_t);
						added_verticies = 0;
						added_indicies = 0;
						new_vertex_to_old_vertex = {};
					}
				}
				if (added_verticies != 0) {
					NEW_SUBMESH.element_count = added_indicies;
					NEW_SUBMESH.vertex_count = added_verticies;

					LODMesh copiedLodMesh = mesh;
					copiedLodMesh.mark_for_delete = false;
					copiedLodMesh.submesh = NEW_SUBMESH;
					copied_output.push_back(copiedLodMesh);
				}

				mesh.mark_for_delete = true;
			}

			if (scd) {
				const int num_bones = scd->get_num_bones();
				for (int j = 0; j < mesh.submesh.vertex_count; j++) {
					int index = mesh.submesh.base_vertex + j;

					FATVertex& fv = mcd.verticies.at(index);

					for (int x = 0; x < 4; x++) {
						assert(fv.bone_index[x] >= -1 && fv.bone_index[x] < num_bones);
						if (fv.bone_index[x] != -1) {

							bone_is_referenced.at(fv.bone_index[x]) = true;
						}
					}
				}
			}
		}

		for (int i = 0; i < lod.mesh_nodes.size(); i++)
			copied_output[i] = lod.mesh_nodes[i];
		lod.mesh_nodes = copied_output;
	}

	// testing: lods via meshopt
	if (def.generate_auto_lods) {
		sys_print(Info, "generating automatic lods\n");

		const int NUM_LODS_TO_GEN = 4;
		for (int lod_gen = 0; lod_gen < NUM_LODS_TO_GEN; lod_gen++) {
			CompileModLOD newLod;
			newLod.share_verticies_with_lod0 = true;
			const CompileModLOD lod0 = mcd.lod_where.at(mcd.lod_where.size() - 1);

			int wants_stop_count = 0;
			for (int part = 0; part < lod0.mesh_nodes.size(); part++) {
				const auto& part_obj = lod0.mesh_nodes.at(part);
				if (part_obj.mark_for_delete)
					continue;
				const int target_i = part_obj.submesh.element_count >> 1;
				const int orig_indicies_index = part_obj.submesh.element_offset / sizeof(uint32_t);
				const int vertex_i = part_obj.submesh.base_vertex;
				const int num_i = part_obj.submesh.element_count;
				std::vector<uint32_t> indicies(num_i);

				float out_er = 0.0;

				float err_allowed = 0.1;
				for (int i = 0; i < lod_gen; i++)
					err_allowed *= 0.6f;

				const auto attribute_weights = {1.f, 1.f, 1.f};
				int result_count = (int)meshopt_simplifyWithAttributes(
					indicies.data(),
					(uint32_t*)&mcd.indicies.at(orig_indicies_index),
					num_i,
					(float*)&mcd.verticies.at(vertex_i).position.x,
					part_obj.submesh.vertex_count,
					sizeof(FATVertex),
					(float*)&mcd.verticies.at(vertex_i).normal.x,
					sizeof(FATVertex),
					attribute_weights.begin(),
					attribute_weights.size(),
					nullptr,
					target_i,
					err_allowed,
					meshopt_SimplifyPrune,
					&out_er);

				bool append_original = false;
				if ((float(result_count) / num_i) > 0.8) {

					result_count = (int)meshopt_simplifySloppy(
						indicies.data(), (uint32_t*)&mcd.indicies.at(orig_indicies_index), num_i,
						(float*)&mcd.verticies.at(vertex_i), part_obj.submesh.vertex_count, sizeof(FATVertex), target_i,
						err_allowed, &out_er);

					if ((float(result_count) / num_i) > 0.8) {
						wants_stop_count += 1;
						append_original = true;
					}
				}

				if (result_count == 0) {
					wants_stop_count += 1;
					break;
				}

				LODMesh new_part = part_obj;
				if (!append_original) {
					new_part.submesh.element_count = result_count;
					new_part.submesh.element_offset = mcd.indicies.size() * sizeof(uint32_t);
					for (int i = 0; i < result_count; i++) {
						mcd.indicies.push_back(indicies.at(i));
					}
				}
				newLod.mesh_nodes.push_back(new_part);
			}

			if (wants_stop_count == lod0.mesh_nodes.size())
				break;

			mcd.lod_where.push_back(newLod);
		}
	}

	int FINAL_bone_count = 0;
	std::vector<int> FINAL_to_LOAD_bones;
	std::vector<int> LOAD_to_FINAL_bones;
	if (scd) {
		const int num_bones = scd->get_num_bones();
		for (int i = 0; i < num_bones; i++) {
			mark_used_bones_R(i, scd, bone_is_referenced);
		}
		LOAD_to_FINAL_bones.resize(num_bones, -1);
		int count = 0;
		for (int i = 0; i < num_bones; i++) {
			if (bone_is_referenced[i]) {
				LOAD_to_FINAL_bones[i] = count++;
			} else {
				if (modcompile_print_pruned_bones.get_bool())
					sys_print(Info, "bone will be pruned %s\n", scd->bones[i].strname.c_str());
			}
		}
		FINAL_to_LOAD_bones.resize(count);
		for (int i = 0; i < LOAD_to_FINAL_bones.size(); i++)
			if (LOAD_to_FINAL_bones[i] != -1)
				FINAL_to_LOAD_bones.at(LOAD_to_FINAL_bones[i]) = i;

		FINAL_bone_count = count;
		sys_print(Info, "final bone count %d\n", FINAL_bone_count);

		for (int i = 0; i < mcd.lod_where.size(); i++) {
			if (mcd.lod_where[i].share_verticies_with_lod0)
				continue;

			for (int j = 0; j < mcd.lod_where[i].mesh_nodes.size(); j++) {
				auto& mesh = mcd.lod_where[i].mesh_nodes[j];
				if (mesh.mark_for_delete)
					continue;
				for (int k = 0; k < mesh.submesh.vertex_count; k++) {
					int index = mesh.submesh.base_vertex + k;
					FATVertex& fv = mcd.verticies.at(index);
					for (int l = 0; l < 4; l++) {
						if (fv.bone_index[l] == -1)
							continue;
						assert(LOAD_to_FINAL_bones[fv.bone_index[l]] != -1);
						fv.bone_index[l] = LOAD_to_FINAL_bones[fv.bone_index[l]];
					}
				}
			}
		}

		assert(FINAL_bone_count > 0 && scd->get_bone_parent(LOAD_to_FINAL_bones[0]) == -1);
		for (int i = 0; i < FINAL_bone_count; i++) {
			const int FINAL_bone = i;
			const int LOAD_bone = FINAL_to_LOAD_bones[i];
			assert(LOAD_bone != -1);
			const int LOAD_parent = scd->get_bone_parent(LOAD_bone);
			const int FINAL_parent = (LOAD_parent == -1) ? -1 : LOAD_to_FINAL_bones[LOAD_parent];
			assert(FINAL_parent < FINAL_bone);
		}
	}

	ProcessMeshOutput output;
	output.FINAL_bone_to_LOAD_bone = std::move(FINAL_to_LOAD_bones);
	output.LOAD_bone_to_FINAL_bone = std::move(LOAD_to_FINAL_bones);
	output.material_is_used = std::move(material_is_used);

	return output;
}

ConfigVar mod_meshopt_run("mod.meshopt", "1", CVAR_BOOL, "");
ConfigVar mod_meshopt_run2("mod.meshopt2nd", "1", CVAR_BOOL, "");

ProcessNodesAndMeshOutput process_nodes_and_mesh(cgltf_data* data, const SkeletonCompileData* scd,
												 const cgltf_skin* using_skin, const ModelDefData& def) {
	ModelCompileData mcd;
	mcd.gltf_file = data;
	cgltf_scene* scene = data->scene;
	for (int i = 0; i < scene->nodes_count; i++) {
		cgltf_node* node = scene->nodes[i];
		traverse_model_nodes(def, mcd, using_skin, node, glm::mat4(1.f));
	}

	auto run_meshopt = [&](bool for_autogen) {
		for (int i = 0; i < mcd.lod_where.size(); i++) {
			auto& lod = mcd.lod_where.at(i);
			if (lod.share_verticies_with_lod0 != for_autogen)
				continue;
			auto& parts = lod.mesh_nodes;
			int part_index = -1;
			for (auto& part : parts) {
				part_index += 1;
				auto& mesh = part.submesh;

				uint32_t* source_indicies = &mcd.indicies.at(mesh.element_offset / sizeof(uint32_t));
				{
					std::vector<unsigned> dest(mesh.element_count);
					meshopt_optimizeVertexCacheFifo(dest.data(), source_indicies, mesh.element_count, mesh.vertex_count,
													16);
					for (int j = 0; j < dest.size(); j++) {
						source_indicies[j] = dest.at(j);
					}
				}

				if (mod_meshopt_run2.get_bool()) {
					if (!lod.share_verticies_with_lod0) {
						std::vector<FATVertex> outFatVert(mesh.vertex_count);
						FATVertex* source_v = &mcd.verticies.at(mesh.base_vertex);
						const size_t out_v_count =
							meshopt_optimizeVertexFetch(outFatVert.data(), source_indicies, mesh.element_count,
														source_v, mesh.vertex_count, sizeof(FATVertex));
						for (int i = 0; i < out_v_count; i++) {
							source_v[i] = outFatVert.at(i);
						}
						mesh.vertex_count = out_v_count;
					}
				}
			}
		}
	};

	if (mod_meshopt_run.get_bool()) {
		run_meshopt(false);
	} else {
		sys_print(Warning, "meshoptimizer disabled, skipping...\n");
	}
	ProcessMeshOutput post_mesh_process = ModelCompileHelper::process_mesh(mcd, scd, def);
	if (mod_meshopt_run.get_bool()) {
		run_meshopt(true);
	}

	ProcessNodesAndMeshOutput output;
	output.mcd = std::move(mcd);
	output.meshout = std::move(post_mesh_process);
	return output;
}

#endif
