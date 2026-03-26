#include "DrawLocal.h"
#include "Model.h"
#include "ModelManager.h"
#include "tracy/public/tracy/Tracy.hpp"

extern ConfigVar r_taa_enabled;
extern ConfigVar r_debug_mode;
extern ConfigVar r_no_batching;
extern ConfigVar r_better_depth_batching;
extern ConfigVar r_skinned_mats_bone_buffer_size;

Render_Pass::Render_Pass(pass_type type) : type(type) {}

draw_call_key Render_Pass::create_sort_key_from_obj(const Render_Object& proxy, const MaterialInstance* material,
													uint32_t camera_dist, int submesh, int layer, bool is_editor_mode) {
	draw_call_key key{};

#ifdef _DEBUG
	const bool is_depth = !r_ignore_depth_shader.get_bool() && (type == pass_type::DEPTH);
	assert(proxy.model);
#else
	const bool is_depth = type == pass_type::DEPTH;
#endif

	int flags = 0;
	// do some if/else here to cut back on permutation insanity. depth only doesnt care about lightmap,taa,editor_id, or
	// debug
	if (proxy.animator_bone_ofs != -1 && proxy.model && proxy.model->has_bones())
		flags |= MSF_ANIMATED;
	if (is_depth) {
		flags |= MSF_DEPTH_ONLY;
	} else if (forced_forward) {
		flags |= MSF_IS_FORCED_FORWARD;
	} else {
		if (proxy.lightmapped)
			flags |= MSF_LIGHTMAPPED;
		if (is_editor_mode)
			flags |= MSF_EDITOR_ID;
		if (!r_taa_enabled.get_bool())
			flags |= MSF_NO_TAA;
		if (r_debug_mode.get_integer() != 0)
			flags |= MSF_DEBUG;
	}

	key.shader = matman.get_mat_shader(proxy.model, material, flags);
	const MasterMaterialImpl* mm = material->get_master_material();

	key.blending = (uint64_t)mm->blend;
	key.backface = mm->backface;
	key.texture = material->impl->get_texture_id_hash();

	VaoType theVaoType = VaoType::Animated;
	if (proxy.lightmapped)
		theVaoType = VaoType::Lightmapped;

	key.vao = (int)theVaoType;
	key.mesh = proxy.model->get_uid();

	if (proxy.is_skybox)
		key.layer = 2; // make skybox last, saves frame time
	else if (proxy.sort_first)
		key.layer = 0;
	else
		key.layer = 1;

	if (mm->blend != BlendState::OPAQUE)
		key.distance = camera_dist;

	return key;
}

void Render_Pass::add_object(const Render_Object& proxy, handle<Render_Object> handle, const MaterialInstance* material,
							 uint32_t camera_dist, int submesh, int lod, int layer, bool is_editor_mode) {
	ASSERT(handle.is_valid() && "null handle");
	ASSERT(material && "null material");
	ZoneScopedN("add_object");
	Pass_Object obj;
	obj.sort_key = create_sort_key_from_obj(proxy, material, camera_dist, submesh, layer, is_editor_mode);
	obj.render_obj = handle;
	obj.submesh_index = submesh;
	obj.material = material;
	obj.lod_index = lod;

	// ensure this material maps to a gpu material
	if (material->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING)
		objects.push_back(obj);
}

#include <iterator>
void Render_Pass::make_batches(Render_Scene& scene) {
	const auto& merge_functor = [](const Pass_Object& a, const Pass_Object& b) {
		if (a.sort_key.as_uint64() < b.sort_key.as_uint64())
			return true;
		else if (a.sort_key.as_uint64() == b.sort_key.as_uint64())
			return a.submesh_index < b.submesh_index;
		else
			return false;
	};

	// objects were added correctly in back to front order, just sort by layer
	const auto& sort_functor_transparent = [](const Pass_Object& a, const Pass_Object& b) {
		if (a.sort_key.blending != b.sort_key.blending)
			return a.sort_key.blending < b.sort_key.blending;
		if (a.sort_key.distance != b.sort_key.distance)
			return a.sort_key.distance > b.sort_key.distance;
		else if (a.sort_key.as_uint64() != b.sort_key.as_uint64())
			return a.sort_key.as_uint64() < b.sort_key.as_uint64();
		return a.submesh_index < b.submesh_index;
	};

	if (type == pass_type::TRANSPARENT)
		std::sort(objects.begin(), objects.end(), sort_functor_transparent);
	else
		std::sort(objects.begin(), objects.end(), merge_functor);

	batches.clear();
	mesh_batches.clear();

	if (objects.empty())
		return;

	{
		auto functor = [](int first, Pass_Object* po, const Render_Object* rop) -> Mesh_Batch {
			Mesh_Batch batch;
			batch.first = first;
			batch.count = 1;
			// auto& mats = rop->mats;
			int index = rop->model->get_part(po->submesh_index)
							.material_idx; // rop->mesh->parts.at(po->submesh_index).material_idx;
			batch.material = po->material;
			// batch.shader_index = po->sort_key.shader;
			return batch;
		};

		const bool no_batching_dbg = r_no_batching.get_bool();

		// build mesh batches first
		Pass_Object* batch_obj = &objects.at(0);
		const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
		Mesh_Batch batch = functor(0, batch_obj, batch_proxy);
		batch_obj->batch_idx = 0;

		for (int i = 1; i < objects.size(); i++) {
			Pass_Object* this_obj = &objects[i];
			const Render_Object* this_proxy = &scene.get(this_obj->render_obj);
			const bool same_mesh = this_obj->sort_key.mesh == batch_obj->sort_key.mesh;
			const bool same_shader = this_obj->sort_key.shader == batch_obj->sort_key.shader;

			const bool same_submesh = this_obj->submesh_index == batch_obj->submesh_index;
			const bool same_material = this_obj->material == batch_obj->material;
			const bool can_be_merged = !no_batching_dbg && same_material && same_mesh && same_shader && same_submesh &&
									   type != pass_type::TRANSPARENT; // dont merge transparent meshes into instances
			if (can_be_merged)
				batch.count++;
			else {
				mesh_batches.push_back(batch);
				batch = functor(i, this_obj, this_proxy);
				batch_obj = this_obj;
				batch_proxy = this_proxy;
			}
			this_obj->batch_idx = mesh_batches.size();
		}
		mesh_batches.push_back(batch);
	}

	Multidraw_Batch batch;
	batch.first = 0;
	batch.count = 1;

	Mesh_Batch* mesh_batch = &mesh_batches[0];
	Pass_Object* batch_obj = &objects[mesh_batch->first];
	const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);

	const bool use_better_depth_batching = r_better_depth_batching.get_bool();

	for (int i = 1; i < mesh_batches.size(); i++) {
		Mesh_Batch* this_batch = &mesh_batches[i];
		Pass_Object* this_obj = &objects[this_batch->first];
		const Render_Object* this_proxy = &scene.get(this_obj->render_obj);

		bool batch_this = false;

		bool same_layer = batch_obj->sort_key.layer == this_obj->sort_key.layer;
		bool same_vao = batch_obj->sort_key.vao == this_obj->sort_key.vao;
		bool same_material = batch_obj->sort_key.texture == this_obj->sort_key.texture;
		bool same_shader = batch_obj->sort_key.shader == this_obj->sort_key.shader;
		bool same_other_state = batch_obj->sort_key.blending == this_obj->sort_key.blending &&
								batch_obj->sort_key.backface == this_obj->sort_key.blending;

		if (type == pass_type::OPAQUE || type == pass_type::TRANSPARENT || !use_better_depth_batching) {
			if (same_vao && same_material && same_other_state && same_shader && same_layer)
				batch_this = true; // can batch with different meshes
			else
				batch_this = false;

		} else { // pass==DEPTH
			// can batch across texture changes as long as its not alpha tested
			if (same_shader && same_vao && same_other_state &&
				!this_batch->material->impl->get_master_impl()->is_alphatested())
				batch_this = true;
			else
				batch_this = false;
		}

		if (batch_this) {
			batch.count += 1;
		} else {
			batches.push_back(batch);
			batch.count = 1;
			batch.first = i;

			mesh_batch = this_batch;
			batch_obj = this_obj;
			batch_proxy = this_proxy;
		}
	}

	batches.push_back(batch);
}

Render_Scene::Render_Scene()
	: gbuffer_pass(pass_type::OPAQUE), transparent_pass(pass_type::TRANSPARENT),
	  // shadow_pass(pass_type::DEPTH),
	  editor_sel_pass(pass_type::DEPTH), shadow_pass(pass_type::DEPTH), depth_prepass(pass_type::DEPTH) {}

void Render_Lists::init(uint32_t drawbufsz, uint32_t instbufsz) {
	glCreateBuffers(1, &gldrawid_to_submesh_material);
	glCreateBuffers(1, &glinstance_to_instance);
	glCreateBuffers(1, &gpu_command_list);
}

void Render_Lists::build_from(Render_Pass& src, Free_List<ROP_Internal>& proxy_list,
							  std::span<uint32_t> draw_to_material) {
	// This function essentially just loops over all batches and creates gpu commands for them
	// its O(n) to the number of batches, not n^2 which it kind of looks like it is

	commands.clear();
	command_count.clear();

	const int max_draw_to_materials = 20000;

	int draw_to_material_index = 0;

	int base_instance = 0;
	int new_verts_drawn = 0;
	for (int i = 0; i < src.batches.size(); i++) {
		const Multidraw_Batch& mdb = src.batches[i];

		for (int j = 0; j < mdb.count; j++) {
			const Mesh_Batch& meshb = src.mesh_batches[mdb.first + j];
			const Pass_Object& obj = src.objects[meshb.first];
			const Render_Object& proxy = proxy_list.get(obj.render_obj.id).proxy;

			const Submesh& part = proxy.model->get_part(obj.submesh_index); // mesh.parts[obj.submesh_index];
			gpu::DrawElementsIndirectCommand cmd;

			cmd.baseVertex = part.base_vertex + proxy.model->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + proxy.model->get_merged_index_ptr();
			cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

			// Important! Set primCount to 0 because visible instances will increment this
			cmd.primCount = 0; // meshb.count;
			cmd.baseInstance = base_instance;

			commands.push_back(cmd);

			base_instance += meshb.count;

			const MaterialInstance* const batch_material = meshb.material;

			assert(draw_to_material_index < src.mesh_batches.size());
			draw_to_material[draw_to_material_index++] = batch_material->impl->gpu_buffer_offset;

			new_verts_drawn += meshb.count * cmd.count;
		}

		command_count.push_back(mdb.count);
	}

	draw.stats.tris_drawn += new_verts_drawn / 3;
}

void Render_Scene::init() {
	gbuffer_rlist.init(0, 0);
	transparent_rlist.init(0, 0);
	// csm_shadow_rlist.init(0,0);
	editor_sel_rlist.init(0, 0);
	cascades_rlists.resize(CascadeShadowMapSystem::CASCADES_USED);
	for (auto& c : cascades_rlists)
		c.init(0, 0);
	spotLightShadowList.init(0, 0);

	depth_prepass_rlist.init(0, 0);

	gpu_instance_buffer = IGraphicsDevice::inst->create_buffer({});

	// glCreateBuffers(1, &gpu_render_instance_buffer);
	glCreateBuffers(1, &gpu_skinned_mats_buffer);

	gpu_skinned_mats_buffer_size = r_skinned_mats_bone_buffer_size.get_integer();
	glNamedBufferData(gpu_skinned_mats_buffer, gpu_skinned_mats_buffer_size * sizeof(glm::mat4), nullptr,
					  GL_STATIC_DRAW);
}

void Render_Scene::update_obj(handle<Render_Object> handle, const Render_Object& proxy) {
	ASSERT(!eng->get_is_in_overlapped_period());
	ROP_Internal& in = proxy_list.get(handle.id);

	if (!(in.proxy.model == proxy.model && in.proxy.mat_override == proxy.mat_override)) {
		int parts = 0;
		if (proxy.model) {
			parts = proxy.model->get_num_parts();
			in.has_transparents = proxy.model->get_has_any_transparent_materials();
		}
		if (proxy.mat_override && proxy.mat_override->impl && proxy.mat_override->impl->is_transparent_material()) {
			in.fastcpu_index = -1; // skip, material is transparent
			in.has_transparents = true;
		}
		// prevent bad case... if too many parts dont take fast path
		else if (parts > 200) {
			in.fastcpu_index = -1;
		} else {
			in.fastcpu_index = BuildSceneData_CpuFast::inst->get_index(proxy.model, proxy.mat_override);
		}
	}

	in.prev_transform = in.proxy.transform;
	in.prev_bone_ofs = in.proxy.animator_bone_ofs;
	in.proxy = proxy;
	if (!in.has_init) {
		in.has_init = true;
		in.prev_transform = in.proxy.transform;
		in.prev_bone_ofs = -1;
	}
	// if (r_disable_animated_velocity_vector.get_bool())
	//	in.prev_bone_ofs = -1;

	if (proxy.model) {
		auto& sphere = proxy.model->get_bounding_sphere();
		auto center = proxy.transform * glm::vec4(glm::vec3(sphere), 1.f);
		float max_scale = glm::max(glm::length(proxy.transform[0]),
								   glm::max(glm::length(proxy.transform[1]), glm::length(proxy.transform[2])));
		float radius = sphere.w * max_scale;
		in.bounding_sphere_and_radius = glm::vec4(glm::vec3(center), radius);
	}
}
ConfigVar r_spot_near("r_spot_near", "0.1", CVAR_FLOAT, "", 0, 2);
void Render_Scene::update_light(handle<Render_Light> handle, const Render_Light& proxy) {
	ASSERT(!eng->get_is_in_overlapped_period());
	auto& l = light_list.get(handle.id);
	l.light = proxy;
	l.updated_this_frame = true;

	if (l.light.casts_shadow_mode != 0 && l.light.is_spotlight) {
		auto& p = l.light.position;
		auto& n = l.light.normal;
		glm::vec3 up = glm::vec3(0, 1, 0);
		if (glm::abs(glm::dot(up, n)) > 0.999)
			up = glm::vec3(1, 0, 0);
		auto viewMat = glm::lookAt(p, p + n, up);
		float fov = glm::radians(l.light.conemax) * 2.0;
		auto proj = glm::perspectiveRH_ZO(fov, 1.0f, l.light.radius, r_spot_near.get_float());
		// proj[2][2] *= -1.0f;	// reverse z // [1,0]
		// proj[3][2] *= -1.0f;

		l.lightViewProj = proj * viewMat;
	}
}

void Render_Scene::remove_light(handle<Render_Light>& handle) {
	if (eng->get_is_in_overlapped_period()) {
		add_to_queued_deletes(handle.id, RenderObjectTypes::Light);
		handle = {-1};
		return;
	}
	if (!handle.is_valid())
		return;
	draw.spotShadows->on_remove_light(handle);
	light_list.free(handle.id);
	handle = {-1};
}
