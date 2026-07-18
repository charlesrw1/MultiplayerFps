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
#include "physx/cooking/PxCooking.h"

#include <fstream>

#include <meshoptimizer.h>

#include <cstring>
#include "Framework/DictWriter.h"

static std::string make_non_proplematic_name(const std::string& name) {
	std::string out;
	if (name.empty())
		return "";
	for (int i = 0; i < name.size(); i++) {
		if (isalnum(name[i]))
			out.push_back(name[i]);
	}
	return out;
}

static void output_embedded_texture(const std::string& outputname, const cgltf_image* i, const cgltf_data* d) {
	sys_print(Info, "writing out embedded texture %s\n", outputname.c_str());

	std::string image_path = outputname;
	auto outfile = FileSys::open_write_game(image_path);

	if (!outfile) {
		sys_print(Error, "couldn't open file to output embedded texture %s\n", image_path.c_str());
		return;
	}

	const cgltf_buffer_view& bv = *i->buffer_view;
	const cgltf_buffer& b = *bv.buffer;

	ASSERT(bv.stride == 0);
	uint8_t* buffer_bytes = (uint8_t*)b.data;

	const char* name = i->name;
	if (!name)
		name = "";

	outfile->write((char*)(buffer_bytes + bv.offset), bv.size);
}

std::vector<std::string> ModelCompileHelper::create_final_material_names(const std::string& modelname,
																		 const ModelCompileData& comp,
																		 const ModelDefData& def,
																		 const std::vector<bool>& materials_used) {

	const int num_materials = comp.gltf_file->materials_count;
	std::vector<std::string> final_mats(num_materials + 1);

	const cgltf_data* d = comp.gltf_file;
	int index_accum = 0;
	for (int i = 0; i < num_materials; i++) {
		if (!materials_used[i])
			continue;

		const auto& cgltf_mat = d->materials[i];
		std::string mat_name = cgltf_mat.name;

		if (def.export_embedded_textures) {
			if (cgltf_mat.pbr_metallic_roughness.base_color_texture.texture)
				output_embedded_texture(strip_extension(modelname) + "_ALB.png",
										cgltf_mat.pbr_metallic_roughness.base_color_texture.texture->image, d);
			if (cgltf_mat.normal_texture.texture)
				output_embedded_texture(strip_extension(modelname) + "_NRM.png", cgltf_mat.normal_texture.texture->image,
										d);
		}

		if (index_accum < def.directMaterialSet.size())
			mat_name = def.directMaterialSet[index_accum];
		if (!mat_name.ends_with(".mi") && !mat_name.ends_with(".mm")) {
			sys_print(Warning, "Exported material %s doesnt end with '.mi' or '.mm', adding it\n", mat_name.c_str());
			mat_name += ".mi";
		}

		final_mats[i] = mat_name;
		index_accum++;
	}
	if (!def.directMaterialSet.empty())
		final_mats[num_materials] = def.directMaterialSet.front();
	else
		final_mats[num_materials] = "_NULL";
	return final_mats;
}

static void add_data_to_vertex_shared(const FATVertex& v, const glm::mat4& transform, const glm::mat3& normal_tr,
							   ModelVertex& mv) {
	mv.pos = v.position;
	mv.pos = transform * glm::vec4(v.position, 1.0);
	mv.uv = v.uv;

	glm::vec3 normal = normal_tr * v.normal;
	normal = glm::normalize(normal);
	for (int i = 0; i < 3; i++) {
		mv.normal[i] = normal[i] * INT16_MAX;
	}

	glm::vec3 tangent = normal_tr * glm::vec3(v.tangent);
	tangent = glm::normalize(tangent);

	for (int i = 0; i < 2; i++) {
		uint16_t z_magnitude = uint16_t(glm::clamp(tangent[i] * 0.5 + 0.5, 0.0, 1.0) * 0x7FFF);
		mv.tangent[i] = *reinterpret_cast<int16_t*>(&z_magnitude);
	}

	const float handedness = v.tangent.w;

	uint16_t sign_bit = (handedness < 0) ? 0x8000 : 0x0000;
	uint16_t z_magnitude = int16_t(glm::clamp(tangent.z * 0.5 + 0.5, 0.0, 1.0) * 0x7FFF);
	uint16_t packed = z_magnitude | sign_bit;
	mv.tangent[2] = *reinterpret_cast<int16_t*>(&packed);
}

static ModelVertex fatvert_to_mv_skinned(const FATVertex& v, const glm::mat4& transform, const glm::mat3& normal_tr) {
	ModelVertex mv;
	add_data_to_vertex_shared(v, transform, normal_tr, mv);

	for (int i = 0; i < 4; i++) {
		mv.color[i] = v.bone_index[i];
	}

	for (int i = 0; i < 4; i++) {
		int qu = v.bone_weight[i] * 255.0;
		if (qu > 255)
			qu = 255;
		if (qu < 0)
			qu = 0;
		mv.color2[i] = qu;
	}

	return mv;
}

static ModelVertex fatvert_to_mv_non_skinned(const FATVertex& v, const glm::mat4& transform, const glm::mat3& normal_tr) {
	ModelVertex mv;
	add_data_to_vertex_shared(v, transform, normal_tr, mv);

	for (int i = 0; i < 4; i++) {
		int qu = v.color[i] * 255.0;
		if (qu > 255)
			qu = 255;
		if (qu < 0)
			qu = 0;
		mv.color2[i] = qu;
	}

	return mv;
}

static ModelVertex fatvert_to_mv_lightmapped(const FATVertex& v, const glm::mat4& transform, const glm::mat3& normal_tr) {
	ModelVertex mv;
	add_data_to_vertex_shared(v, transform, normal_tr, mv);

	int as_uintX = v.uv2.x * UINT16_MAX;
	int as_uintY = v.uv2.y * UINT16_MAX;
	as_uintX = glm::clamp(as_uintX, 0, (int)UINT16_MAX);
	as_uintY = glm::clamp(as_uintY, 0, (int)UINT16_MAX);

	mv.color[0] = (as_uintX & 0xFF);
	mv.color[1] = (as_uintX >> 8) & 0xFF;
	mv.color[2] = (as_uintY & 0xFF);
	mv.color[3] = (as_uintY >> 8) & 0xFF;
	for (int i = 0; i < 4; i++) {
		mv.color2[i] = int(v.color[i] * 255.0);
	}
	return mv;
}

static FinalPhysicsData create_final_physics_data(const std::vector<std::string>& final_mat_names,
										   const std::vector<bool>& mat_is_used, const ModelCompileData& compile,
										   const std::vector<int>& LOAD_to_FINAL_bones, const ModelDefData& def) {
	FinalPhysicsData out;
	std::vector<LODMesh> physicsNodeCopy = compile.physics_nodes;
	if (def.use_mesh_as_collision && def.use_mesh_as_cvx_collision) {
		sys_print(Warning, "create_final_physics_data: both use_mesh_as_collision and use_mesh_as_cvx_collision, "
						   "defaulting to tri mesh\n");
	}

	if (def.use_mesh_as_collision) {
		if (!compile.lod_where.empty()) {
			sys_print(Info, "create_final_physics_data: using tri mesh as collision mesh\n");
			auto& s = compile.lod_where.back();
			for (auto& submesh : s.mesh_nodes) {
				LODMesh node = submesh;
				node.shape_type = ShapeType_e::MeshShape;
				physicsNodeCopy.push_back(node);
			}
		} else {
			sys_print(Warning, "create_final_physics_data: cant use tri mesh as collision mesh, no lods\n");
		}
	} else if (def.use_mesh_as_cvx_collision) {
		if (!compile.lod_where.empty()) {
			sys_print(Info, "create_final_physics_data: using meshes as convex\n");
			auto& s = compile.lod_where.back();
			for (auto& submesh : s.mesh_nodes) {
				LODMesh node = submesh;
				node.shape_type = ShapeType_e::ConvexShape;
				physicsNodeCopy.push_back(node);
			}
		} else {
			sys_print(Warning, "create_final_physics_data: cant use mesh as convex collision mesh, no lods\n");
		}
	}

	for (auto& p : physicsNodeCopy) {
		switch (p.shape_type) {
		case ShapeType_e::ConvexShape: {

			physx::PxConvexMeshDesc desc;
			auto buf = std::make_unique<physx::PxDefaultMemoryOutputStream>();
			physx::PxCookingParams params = physx::PxCookingParams(physx::PxTolerancesScale());
			params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
			params.meshWeldTolerance = 0.001f;

			desc.flags |= physx::PxConvexFlag::eCOMPUTE_CONVEX;

			const int index_start = p.submesh.element_offset / sizeof(uint32_t);
			const int vertex_start = p.submesh.base_vertex;

			glm::mat4 transform = p.ref.globaltransform;
			std::vector<glm::vec3> positions;
			for (int i = 0; i < p.submesh.vertex_count; i++) {
				positions.push_back(
					glm::vec3(transform * glm::vec4(compile.verticies[vertex_start + i].position, 1.0f)));
			}

			desc.points.count = p.submesh.vertex_count;
			desc.points.data = positions.data();
			desc.points.stride = sizeof(glm::vec3);

			std::vector<uint32_t> indicies;

			desc.indices.count = p.submesh.element_count;
			desc.indices.data = ((uint8_t*)(compile.indicies.data() + index_start));
			desc.indices.stride = sizeof(uint32_t);

			physx::PxConvexMeshCookingResult::Enum res;
			bool good = PxCookConvexMesh(params, desc, *buf.get(), &res);

			if (good) {
				physics_shape_def def;
				def.convex_mesh = (physx::PxConvexMesh*)buf.get();
				def.shape = ShapeType_e::ConvexShape;
				out.output_streams.push_back(std::move(buf));
				out.shapes.push_back(def);
			} else
				sys_print(Error, "PxCookConvexMesh failed\n");
		} break;
		case ShapeType_e::MeshShape: {
			physx::PxTriangleMeshDesc desc;
			auto buf = std::make_unique<physx::PxDefaultMemoryOutputStream>();
			physx::PxCookingParams params = physx::PxCookingParams(physx::PxTolerancesScale());
			params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
			params.meshWeldTolerance = 0.001f;

			const int index_start = p.submesh.element_offset / sizeof(uint32_t);
			const int vertex_start = p.submesh.base_vertex;

			glm::mat4 transform = p.ref.globaltransform;
			std::vector<glm::vec3> positions;
			for (int i = 0; i < p.submesh.vertex_count; i++) {
				positions.push_back(
					glm::vec3(transform * glm::vec4(compile.verticies[vertex_start + i].position, 1.0f)));
			}

			desc.points.count = p.submesh.vertex_count;
			desc.points.data = positions.data();
			desc.points.stride = sizeof(glm::vec3);

			desc.triangles.count = p.submesh.element_count / 3;
			desc.triangles.data = ((uint8_t*)(compile.indicies.data() + index_start));
			desc.triangles.stride = sizeof(uint32_t) * 3;

			physx::PxTriangleMeshCookingResult::Enum res;
			bool good = PxCookTriangleMesh(params, desc, *buf.get(), &res);

			if (good) {
				physics_shape_def def;
				def.tri_mesh = (physx::PxTriangleMesh*)buf.get();
				def.shape = ShapeType_e::MeshShape;
				out.output_streams.push_back(std::move(buf));
				out.shapes.push_back(def);
			} else
				sys_print(Error, "PxCookConvexMesh failed\n");
		} break;
		}
	}
	return out;
}

static Submesh make_final_submesh_from_existing(const bool has_skeleton, const Submesh& in, FinalModelData& finalmod,
										 const ModelCompileData& compile, const std::vector<int>& indirect_mats_out,
										 glm::mat4 transform, const bool dont_append_verticies) {
	Submesh out;
	out.material_idx =
		(in.material_idx == -1) ? indirect_mats_out[indirect_mats_out.size() - 1] : indirect_mats_out[in.material_idx];
	out.element_count = in.element_count;
	out.vertex_count = in.vertex_count;

	glm::mat4 invtransform = glm::inverse(transform);
	glm::mat3 normal_tr = glm::mat3(glm::transpose(invtransform));

	const int index_start = in.element_offset / sizeof(uint32_t);
	const int new_index_start = finalmod.indicies.size();
	for (int i = 0; i < in.element_count; i++) {
		finalmod.indicies.push_back(compile.indicies[index_start + i]);
	}
	const int vertex_start = in.base_vertex;
	const int new_vertex_start = finalmod.verticies.size();

	if (!dont_append_verticies) {
		if (finalmod.get_is_lightmapped_bool()) {
			for (int i = 0; i < in.vertex_count; i++) {
				finalmod.verticies.push_back(
					fatvert_to_mv_lightmapped(compile.verticies[vertex_start + i], transform, normal_tr));
			}
		} else if (has_skeleton) {
			for (int i = 0; i < in.vertex_count; i++) {
				finalmod.verticies.push_back(
					fatvert_to_mv_skinned(compile.verticies[vertex_start + i], transform, normal_tr));
			}
		} else {
			for (int i = 0; i < in.vertex_count; i++) {
				finalmod.verticies.push_back(
					fatvert_to_mv_non_skinned(compile.verticies[vertex_start + i], transform, normal_tr));
			}
		}
		out.base_vertex = new_vertex_start;
	}

	out.element_offset = new_index_start * sizeof(uint16_t);

	return out;
}

static FinalModelData create_final_model_data(const FinalSkeletonOutput* skel, const std::vector<std::string>& final_mat_names,
									   const std::vector<bool>& mat_is_used,
									   const ModelCompileData& compile, const std::vector<int>& LOAD_to_FINAL_bones,
									   const ModelDefData& def) {
	FinalModelData final_mod;
	final_mod.final_physics =
		create_final_physics_data(final_mat_names, mat_is_used, compile, LOAD_to_FINAL_bones, def);

	std::vector<int> indirect_mats_out(final_mat_names.size(), -1);
	int count = 0;
	for (int i = 0; i < mat_is_used.size(); i++) {
		if (mat_is_used[i]) {
			indirect_mats_out[i] = count++;
			final_mod.material_names.push_back(final_mat_names[i]);
		}
	}

	const int num_actual_lods = compile.lod_where.size();
	std::vector<int> lods_to_def(num_actual_lods, -1);
	for (int i = 0; i < def.loddefs.size(); i++) {
		assert(def.loddefs[i].lod_num >= 0);
		if (def.loddefs[i].lod_num >= num_actual_lods)
			continue;
		lods_to_def[def.loddefs[i].lod_num] = i;
	}
	final_mod.isLightmapped = Model::LightmapType::None;
	if (def.isLightmapped) {
		if (skel != nullptr) {
			sys_print(Warning, "create_final_model_data: %s isLightmapped not compatible with a skeletal mesh.\n",
					  def.model_source.c_str());
		} else {
			if (def.worldLmMerge)
				final_mod.isLightmapped = Model::LightmapType::WorldMerged;
			else
				final_mod.isLightmapped = Model::LightmapType::Lightmapped;

			final_mod.lightmapX = def.lightmapSizeX;
			final_mod.lightmapY = def.lightmapSizeY;
			if (def.lightmapSizeX == 0 || def.lightmapSizeY == 0) {
				if (def.lightmapSizeX == 0)
					final_mod.lightmapX = 16;
				if (def.lightmapSizeY == 0)
					final_mod.lightmapY = 16;
				sys_print(Warning, "create_final_model_data: %s lightmapSize was 0, setting to default 16.\n",
						  def.model_source.c_str());
			}
		}
	}

	Bounds total_bounds;
	for (int i = 0; i < compile.lod_where.size(); i++) {
		MeshLod out_lod;

		if (i != 0) {
			if (lods_to_def.at(i) == -1) {
				sys_print(Error,
						  "create_final_model_data: mesh has LOD_%d parts, but distance wasn't definied in .def, "
						  "skipping...\n",
						  i);
				continue;
			}
			out_lod.end_percentage = def.loddefs.at(lods_to_def.at(i)).distance;
		} else
			out_lod.end_percentage = 1.0;

		out_lod.part_ofs = final_mod.submeshes.size();
		auto& lod = compile.lod_where[i];
		for (int j = 0; j < lod.mesh_nodes.size(); j++) {
			if (lod.mesh_nodes[j].mark_for_delete)
				continue;

			const LODMesh& lm = lod.mesh_nodes[j];

			if (final_mod.get_is_lightmapped_bool() && !lm.has_attribute(CMA_UV2)) {
				sys_print(Warning, "create_final_model_data: isLightmapped=true but mesh doesnt have UV2? %s\n",
						  def.model_source.c_str());
			}

			glm::mat4 transform = lm.ref.globaltransform;
			if (lm.has_bones())
				transform = glm::mat4(1.0);

			glm::vec3 a = transform * glm::vec4(lm.bounds.bmin, 1.0);
			glm::vec3 b = transform * glm::vec4(lm.bounds.bmax, 1.0);
			Bounds bounds = Bounds(a);
			bounds = bounds_union(bounds, b);

			total_bounds = bounds_union(total_bounds, bounds);

			final_mod.submeshes.push_back(make_final_submesh_from_existing(skel != nullptr, lm.submesh, final_mod,
																		   compile, indirect_mats_out, transform,
																		   lod.share_verticies_with_lod0));
			if (lod.share_verticies_with_lod0) {
				auto& lod0 = final_mod.lods.at(0);
				ASSERT(lod0.part_count == lod.mesh_nodes.size());
				auto& submesh = final_mod.submeshes.at(lod0.part_ofs + j);
				auto& my_submesh = final_mod.submeshes.back();
				my_submesh.base_vertex = submesh.base_vertex;
			}
		}
		out_lod.part_count = final_mod.submeshes.size() - out_lod.part_ofs;

		final_mod.lods.push_back(out_lod);
	}

	if (final_mod.lods.size() == 0) {
		sys_print(Warning, "model has no lods to output, creating an empty default one\n");
		MeshLod loddefault;
		loddefault.part_count = 0;
		loddefault.part_ofs = 0;
		final_mod.lods.push_back(loddefault);
		total_bounds = Bounds(glm::vec3(-0.5), glm::vec3(0.5));
	}

	final_mod.AABB = total_bounds;
	final_mod.cullScreenSize = def.cullScreenSize;

	return final_mod;
}

static bool write_out_compilied_model(const std::string& gamepath, const FinalModelData* model,
							   const FinalSkeletonOutput* skel) {
	FileWriter out;
	out.write_int32('CMDL');
	out.write_int32(MODEL_VERSION);

	out.write_byte((uint8_t)model->isLightmapped);
	out.write_int32(model->lightmapX);
	out.write_int32(model->lightmapY);

	glm::mat4 roottransform = glm::mat4(1.0);
	if (skel)
		roottransform = skel->armature_root_transform;
	out.write_struct(&roottransform);

	out.write_struct(&model->AABB);

	out.write_int32(model->lods.size());
	for (int i = 0; i < model->lods.size(); i++)
		out.write_struct(&model->lods[i]);
	out.write_float(model->cullScreenSize);
	out.write_int32(model->submeshes.size());
	for (int i = 0; i < model->submeshes.size(); i++)
		out.write_struct(&model->submeshes[i]);
	out.write_int32('HELP');
	out.write_int32(model->material_names.size());
	for (int i = 0; i < model->material_names.size(); i++)
		out.write_string(model->material_names[i]);
	out.write_int32(model->tags.size());
	for (int i = 0; i < model->tags.size(); i++) {
		out.write_string(model->tags[i].name);
		out.write_struct(&model->tags[i].transform);
		out.write_int32(model->tags[i].bone_index);
	}

	size_t marker = out.tell();
	out.write_int32(model->indicies.size());
	out.write_bytes_ptr((uint8_t*)model->indicies.data(), model->indicies.size() * sizeof(uint16_t));
	size_t index_size = out.tell() - marker;

	marker = out.tell();
	sys_print(Debug, "MARKER: %d\n", (int)marker);
	out.write_int32(model->verticies.size());
	out.write_bytes_ptr((uint8_t*)model->verticies.data(), model->verticies.size() * sizeof(ModelVertex));
	size_t vert_size = out.tell() - marker;

	out.write_int32('HELP');

	if (model->final_physics.shapes.size() > 0) {
		auto& body = model->final_physics;
		out.write_byte(1);
		out.write_int32(body.shapes.size());
		for (int i = 0; i < body.shapes.size(); i++) {
			out.write_bytes_ptr((uint8_t*)&body.shapes[i], sizeof(physics_shape_def));
			if (body.shapes[i].shape == ShapeType_e::ConvexShape) {
				physx::PxDefaultMemoryOutputStream* output =
					(physx::PxDefaultMemoryOutputStream*)body.shapes[i].convex_mesh;
				out.write_int32(output->getSize());
				out.write_bytes_ptr(output->getData(), output->getSize());
			} else if (body.shapes[i].shape == ShapeType_e::MeshShape) {
				physx::PxDefaultMemoryOutputStream* output =
					(physx::PxDefaultMemoryOutputStream*)body.shapes[i].tri_mesh;
				out.write_int32(output->getSize());
				out.write_bytes_ptr(output->getData(), output->getSize());
			}
			out.write_int32('HELP');
		}
	} else {
		out.write_byte(0);
	}

	out.write_int32('HELP');

	size_t skel_size = 0;
	size_t animation_size = 0;

	if (!skel)
		out.write_int32(0);
	else {

		marker = out.tell();
		out.write_int32(skel->bones.size());
		for (int i = 0; i < skel->bones.size(); i++) {
			out.write_string(skel->bones[i].strname);
			out.write_int32(skel->bones[i].parent);
			out.write_int32((int)skel->bones[i].retarget_type);
			out.write_struct(&skel->bones[i].posematrix);
			out.write_struct(&skel->bones[i].invposematrix);
			out.write_struct(&skel->bones[i].localtransform);
			out.write_struct(&skel->bones[i].rot);
		}
		skel_size = out.tell() - marker;

		marker = out.tell();
		out.write_int32(skel->allseqs.size());
		for (const auto& seq : skel->allseqs) {
			out.write_int32('HELP');

			out.write_string(seq.first);
			out.write_float(seq.second.duration);
			out.write_float(seq.second.average_linear_velocity);
			out.write_int32(seq.second.num_frames);
			out.write_byte(seq.second.is_additive_clip);
			out.write_byte(seq.second.has_rootmotion);

			assert(seq.second.channel_offsets.size() == skel->bones.size());
			out.write_bytes_ptr((uint8_t*)seq.second.channel_offsets.data(),
								seq.second.channel_offsets.size() * sizeof(ChannelOffset));

			out.write_int32(seq.second.pose_data.size());
			out.write_bytes_ptr((uint8_t*)seq.second.pose_data.data(), seq.second.pose_data.size() * sizeof(float));
		}
		animation_size = out.tell() - marker;

		out.write_int32(skel->imported_models.size());
		for (int i = 0; i < skel->imported_models.size(); i++)
			out.write_string(skel->imported_models[i]);

		out.write_byte(skel->mirror_table.size() == skel->bones.size());
		if (skel->mirror_table.size() == skel->bones.size()) {
			out.write_bytes_ptr((uint8_t*)skel->mirror_table.data(), skel->bones.size() * sizeof(int16_t));
		}

		out.write_int32('E');
	}

	auto outfile = FileSys::open_write_game(gamepath);
	if (!outfile) {
		sys_print(Error, "Couldn't open file to write out model %s\n", gamepath.c_str());
		return false;
	}
	sys_print(Debug, "Writing out model (%s) (size: %d)\n", gamepath.c_str(), (int)out.get_size());
	sys_print(Debug, "    -vert bytes: %d\n", (int)vert_size);
	sys_print(Debug, "    -index bytes: %d\n", (int)index_size);
	sys_print(Debug, "    -bone bytes: %d\n", (int)skel_size);
	sys_print(Debug, "    -anim bytes: %d\n", (int)animation_size);

	outfile->write(out.get_buffer(), out.get_size());
	outfile->close();

	return true;
}

static void add_bone_def_data_to_skeleton(const ModelDefData& def, SkeletonCompileData* skel) {
	for (int i = 0; i < skel->get_num_bones(); i++) {
		auto name = skel->bones[i].strname;
		if (def.bone_retarget_type.find(name) != def.bone_retarget_type.end())
			skel->bones[i].retarget_type = def.bone_retarget_type.find(name)->second;
		else
			skel->bones[i].retarget_type = RetargetBoneType::FromTargetBindPose;
	}
}

ModelCompilier::Ret ModelCompileHelper::compile_model(const std::string& defname, const ModelDefData& def) {
	sys_print(Info, "#### Compiling Model %s ####\n", defname.c_str());

	cgltf_and_binary out = load_cgltf_data(def.model_source);
	if (!out.data) {
		sys_print(Error, "load_cgltf_data failed\n");
		return ModelCompilier::CompileErr;
	}

	std::string finalpath = strip_extension(defname);
	finalpath += ".cmdl";

	unique_ptr<SkeletonCompileData> skeleton_data = get_skin_from_file(out.data, defname.c_str(), def.armature_name);
	if (skeleton_data) {
		add_bone_def_data_to_skeleton(def, skeleton_data.get());
		if (def.apply_armature_transform)
			ModelCompileHelper::apply_armature_root_to_skeleton(skeleton_data.get());
	}

	const ProcessNodesAndMeshOutput post_traverse = process_nodes_and_mesh(
		out.data, skeleton_data.get(), (skeleton_data) ? skeleton_data->using_skin : nullptr, def);

	const std::vector<std::string> final_material_names =
		create_final_material_names(defname, post_traverse.mcd, def, post_traverse.meshout.material_is_used);

	unique_ptr<const FinalSkeletonOutput> final_skeleton = ModelCompileHelper::create_final_skeleton(
		finalpath, post_traverse.meshout.LOAD_bone_to_FINAL_bone, post_traverse.meshout.FINAL_bone_to_LOAD_bone,
		skeleton_data.get(), def);

	const FinalModelData final_model =
		create_final_model_data(final_skeleton.get(), final_material_names, post_traverse.meshout.material_is_used,
								post_traverse.mcd, post_traverse.meshout.LOAD_bone_to_FINAL_bone, def);

	bool res = write_out_compilied_model(finalpath, &final_model, final_skeleton.get());

	// Debug dump for skeleton diagnostics
	if (final_skeleton) {
		std::string dumppath = strip_extension(defname) + "_skel_dump.txt";
		std::string fullpath = std::string(FileSys::get_game_path()) + "/" + dumppath;
		std::ofstream dump(fullpath);
		if (dump.is_open()) {
			dump << "=== Skeleton Dump: " << defname << " ===\n";
			dump << "armature_root (after bake):\n";
			if (skeleton_data) {
				auto& ar = skeleton_data->armature_root;
				dump << "  [" << ar[0][0] << ", " << ar[1][0] << ", " << ar[2][0] << ", " << ar[3][0] << "]\n";
				dump << "  [" << ar[0][1] << ", " << ar[1][1] << ", " << ar[2][1] << ", " << ar[3][1] << "]\n";
				dump << "  [" << ar[0][2] << ", " << ar[1][2] << ", " << ar[2][2] << ", " << ar[3][2] << "]\n";
			}
			dump << "\nBones (" << final_skeleton->bones.size() << "):\n";
			for (int i = 0; i < (int)final_skeleton->bones.size() && i < 5; i++) {
				auto& b = final_skeleton->bones[i];
				dump << "[" << i << "] " << b.strname << " parent=" << b.parent << "\n";
				dump << "  localTransform pos: (" << b.localtransform[3][0] << ", " << b.localtransform[3][1] << ", " << b.localtransform[3][2] << ")\n";
				dump << "  localTransform 3x3 col lengths: (" << glm::length(glm::vec3(b.localtransform[0])) << ", " << glm::length(glm::vec3(b.localtransform[1])) << ", " << glm::length(glm::vec3(b.localtransform[2])) << ")\n";
				dump << "  posematrix pos: (" << b.posematrix[3][0] << ", " << b.posematrix[3][1] << ", " << b.posematrix[3][2] << ")\n";
				dump << "  posematrix 3x3 col lengths: (" << glm::length(glm::vec3(b.posematrix[0])) << ", " << glm::length(glm::vec3(b.posematrix[1])) << ", " << glm::length(glm::vec3(b.posematrix[2])) << ")\n";
				dump << "  invposematrix 3x3 col lengths: (" << glm::length(glm::vec3(b.invposematrix[0])) << ", " << glm::length(glm::vec3(b.invposematrix[1])) << ", " << glm::length(glm::vec3(b.invposematrix[2])) << ")\n";
				dump << "  rot: (" << b.rot.x << ", " << b.rot.y << ", " << b.rot.z << ", " << b.rot.w << ")\n";
			}
			dump << "\nVertex sample (first 3):\n";
			for (int i = 0; i < (int)final_model.verticies.size() && i < 3; i++) {
				auto& v = final_model.verticies[i];
				dump << "  [" << i << "] pos=(" << v.pos.x << ", " << v.pos.y << ", " << v.pos.z << ")\n";
			}
			dump << "\nMesh globaltransform for first skinned node:\n";
			for (int l = 0; l < (int)post_traverse.mcd.lod_where.size(); l++) {
				for (int j = 0; j < (int)post_traverse.mcd.lod_where[l].mesh_nodes.size(); j++) {
					auto& lm = post_traverse.mcd.lod_where[l].mesh_nodes[j];
					if (lm.has_bones()) {
						auto& gt = lm.ref.globaltransform;
						dump << "  globaltransform diag: (" << gt[0][0] << ", " << gt[1][1] << ", " << gt[2][2] << ")\n";
						goto done_gt;
					}
				}
			}
			done_gt:
			dump.close();
			sys_print(Info, "wrote skeleton dump to %s\n", fullpath.c_str());
		}
	}

	out.free();
	return res ? ModelCompilier::CompileGood : ModelCompilier::CompileErr;
}

#endif
