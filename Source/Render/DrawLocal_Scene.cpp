#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

glm::vec4 to_vec4(Color32 color) {
	return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
}

inline float get_screen_percentage_2(const glm::vec4& bounding_sphere, float inv_two_times_tanfov_2,
									 float camera_dist_2) {
	return (bounding_sphere.w * bounding_sphere.w) * inv_two_times_tanfov_2 / camera_dist_2;
}
inline float get_shadow_cascade_percentage_2(const glm::vec4& bounding_sphere, float cascade_extent) {
	float texels_per_unit = 1.0 / (cascade_extent);

	float r = bounding_sphere.w * texels_per_unit;

	return r * r;
}

inline const int get_lod_to_render(const Model* model, const float percentage) {
	for (int i = model->get_num_lods() - 1; i > 0; i--) {
		if (percentage <= model->get_lod(i).end_percentage)
			return i;
	}
	return 0;
}

#include "Frustum.h"

ConfigVar r_force_lod("r.force_lod", "-1", CVAR_INTEGER | CVAR_UNBOUNDED, "");

template <bool is_main_view>
static void cull_objects(Frustum& frustum, int visible_array_size, uint8_t* out_array, int16_t* camera_dist,
						 const Free_List<ROP_Internal>& objs_free_list) {
	assert(visible_array_size == objs_free_list.objects.size());
	auto& objs = objs_free_list.objects;

	const int force_lod = r_force_lod.get_integer();

	const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
	const float inv_two_times_tanfov_2 = inv_two_times_tanfov * inv_two_times_tanfov;
	auto& vs = draw.current_frame_view;
	auto& proxy_list = draw.scene.proxy_list;

	// adjust these, maybe exponential depth?
	const float max_cam_dist = 100.0;
	const float inv_max_dist_mult_2 = 1.0 / (max_cam_dist * max_cam_dist);
	const float max_output = float(1 << 12);

	for (int i = 0; i < objs.size(); i++) {
		const auto& obj = objs[i].type_;
		const glm::vec3& center = glm::vec3(obj.bounding_sphere_and_radius);
		const float& radius = obj.bounding_sphere_and_radius.w;

		int res = 0;
		res += (glm::dot(glm::vec3(frustum.top_plane), center) + frustum.top_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.bot_plane), center) + frustum.bot_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.left_plane), center) + frustum.left_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.right_plane), center) + frustum.right_plane.w >= -radius) ? 1 : 0;

		const bool is_visible = res == 4;

		int8_t want_lod = 0;
		if (is_main_view) {
			const glm::vec3 to_camera = center - vs.origin;
			const float dist_to_camera_2 = glm::dot(to_camera, to_camera);
			const float percentage_2 =
				get_screen_percentage_2(obj.bounding_sphere_and_radius, inv_two_times_tanfov_2, dist_to_camera_2);
			if (!obj.proxy.model)
				want_lod = 0;
			else if (force_lod != -1) {
				int lod_to_pick = glm::clamp(force_lod, 0, obj.proxy.model->get_num_lods() - 1);
				want_lod = lod_to_pick;
			} else {
				want_lod = (int8_t)get_lod_to_render(obj.proxy.model, percentage_2);
			}

			float out_dist_cam = dist_to_camera_2 * inv_max_dist_mult_2;
			out_dist_cam = std::clamp(out_dist_cam, 0.f, 1.f);
			int16_t as_int16 = max_output * out_dist_cam;
			camera_dist[i] = as_int16;

		} else {
			const glm::vec3 to_camera = center - vs.origin;
			const float percentage_2 =
				get_shadow_cascade_percentage_2(obj.bounding_sphere_and_radius, frustum.ortho_max_extent);
			if (!obj.proxy.model)
				want_lod = 0;
			else if (force_lod != -1) {
				int lod_to_pick = glm::clamp(force_lod, 0, obj.proxy.model->get_num_lods() - 1);
				want_lod = lod_to_pick;
			} else {
				want_lod = (int8_t)get_lod_to_render(obj.proxy.model, percentage_2);
			}
		}
		pack_input_lod_arr(out_array[i], is_visible, want_lod);
	}
}

void set_gpu_objects_data_job(uintptr_t p) {
	const int current_bone_buffer_offset = draw.scene.get_front_bone_buffer_offset();
	const int prev_bone_buffer_offset = draw.scene.get_back_bone_buffer_offset();

	auto gpu_objects = (uint8*)p;
	auto& proxy_list = draw.scene.proxy_list;
	ZoneScopedN("SetGpuObjectData");
	for (int i = 0; i < proxy_list.objects.size(); i++) {
		auto& obj = proxy_list.objects[i];
		auto& proxy = obj.type_.proxy;

		const int offset = i * 64;
		glm::vec4* v1 = (glm::vec4*)(gpu_objects + offset);
		*v1 = glm::vec4(proxy.transform[0][0], proxy.transform[1][0], proxy.transform[2][0], proxy.transform[3][0]);
		v1 = (glm::vec4*)(gpu_objects + offset + 16);
		*v1 = glm::vec4(proxy.transform[0][1], proxy.transform[1][1], proxy.transform[2][1], proxy.transform[3][1]);
		v1 = (glm::vec4*)(gpu_objects + offset + 32);
		*v1 = glm::vec4(proxy.transform[0][2], proxy.transform[1][2], proxy.transform[2][2], proxy.transform[3][2]);
		glm::ivec4* flags = (glm::ivec4*)(gpu_objects + offset + 48);
		int bone_ofs = proxy.animator_bone_ofs;
		if (bone_ofs >= 0)
			bone_ofs = current_bone_buffer_offset + bone_ofs;
		*flags = glm::ivec4(0, bone_ofs, bone_ofs, 0);
	}
}

void make_batches_job(uintptr_t p) {
	ZoneScopedN("make_batches_job");
	Render_Pass* pass = (Render_Pass*)p;
	pass->make_batches(draw.scene);
}

struct MakeShadowRenderListParam
{
	uint8_t* visarray = nullptr;
	int index = 0;
};

void make_shadow_render_list_job(uintptr_t p) {
	ZoneScopedN("make_shadow_render_list_job");

	auto param = (MakeShadowRenderListParam*)p;

	build_cascade_cpu(draw.scene.cascades_rlists[param->index], draw.scene.shadow_pass, draw.scene.proxy_list,
					  param->visarray);
}

#include "Framework/Jobs.h"

void Render_Scene::refresh_static_mesh_data(bool build_for_editor) {}
ConfigVar test_ignore_bake("test_ignore_bake", "1", CVAR_BOOL, "");
ConfigVar r_debug_transparents("r.debug_transparents", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_force_all_materials_to_fallback("r.force_all_materials_to_fallback", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_dont_use_camera_depth_build_scene("r.dont_use_camera_depth_build_scene", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_depth_prepass("r.skip_depth_prepass", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_depth_prepass_all_objects("r.depth_prepass_all_objects", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_add_to_passes("r.skip_add_to_passes", "0", CVAR_BOOL | CVAR_DEV, "");

void Render_Scene::update_spotlight_data() {
	ZoneScopedN("update_spotlight_data");
	GPUSCOPESTART(update_spotlight_shadows_scope);

	std::vector<handle<Render_Light>> lightsToCalcShadow;
	draw.spotShadows->get_lights_to_render(lightsToCalcShadow);
	draw.stats.shadow_lights += lightsToCalcShadow.size();
	// tbh just do it here whatev

	const int visible_count = draw.scene.proxy_list.objects.size();
	auto& memArena = draw.get_arena();
	if (!lightsToCalcShadow.empty()) {
		// parallelize this

		for (int i = 0; i < lightsToCalcShadow.size(); i++) {
			bool any_dynamic_found = false;
			build_cascade_cpu(spotLightShadowList, draw.scene.shadow_pass, draw.scene.proxy_list, nullptr);
			draw.spotShadows->do_render(spotLightShadowList, lightsToCalcShadow[i], any_dynamic_found);
		}
	}
}

void Render_Scene::build_scene_data(bool skybox_only, bool build_for_editor, bool cubemap_view) {
	GPUSCOPESTART(build_scene_data_scope);
	ZoneScopedN("build_scene_data");

	// ZoneScoped;
	if (r_debug_skip_build_scene_data.get_bool())
		return;

	const bool add_to_passes = !r_skip_add_to_passes.get_bool();

	auto& memArena = draw.get_arena();
	ArenaScope scope(memArena);

	// clear objects
	auto reset_passes = [&]() {
		gbuffer_pass.clear();
		transparent_pass.clear();
		shadow_pass.clear();
		editor_sel_pass.clear();
		depth_prepass.clear();
	};
	if (add_to_passes)
		reset_passes();

	const int visible_count = proxy_list.objects.size();

	// uint8_t* cascade_vis[4] = { nullptr,nullptr,nullptr,nullptr };
	// for(int i=0;i<4;i++)
	//	cascade_vis[i] = memArena.alloc_bottom_type<uint8_t>(visible_count);

	uint8_t* lod_to_render_array = memArena.alloc_bottom_type<uint8_t>(visible_count);
	int16_t* camera_depth_array = memArena.alloc_bottom_type<int16_t>(visible_count);

	{
		CPUSCOPESTART(cpu_object_cull);
		ZoneScopedN("lod_calcs");

		// JobCounter* counter{};
		//
		//
		// const int NUM_FRUSTUM_JOBS = CascadeShadowMapSystem::CASCADES_USED + 1;
		// JobDecl decls[NUM_FRUSTUM_JOBS];
		// CullObjectsUser mainview;
		// CullObjectsUser cascades[CascadeShadowMapSystem::CASCADES_USED];
		// mainview.count = visible_count;
		// mainview.lodarr = lod_to_render_array;
		// mainview.camdistarr = camera_depth_array;
		// decls[0].func = cull_objects_job;
		// decls[0].funcarg = uintptr_t(&mainview);
		// for (int i = 0; i < CascadeShadowMapSystem::CASCADES_USED; i++) {
		//	cascades[i].index = i;
		//	cascades[i].count = visible_count;
		//	cascades[i].lodarr = cascade_vis[i];
		//	decls[i + 1].func = cull_shadow_objects_job;
		//	decls[i + 1].funcarg = uintptr_t(&cascades[i]);
		//}

		// for (int i = 0; i < NUM_FRUSTUM_JOBS; i++)
		//	decls[i].func(decls[i].funcarg);

		// JobSystem::inst->add_jobs(decls, 1, counter);

		// calc_lod_job(lod_to_render_array, camera_depth_array);

		// JobSystem::inst->wait_and_free_counter(counter);
	}
	const size_t num_ren_objs = proxy_list.objects.size();
	uint8* gpu_objects = memArena.alloc_bottom_type<uint8>(num_ren_objs * 64);
	ASSERT(gpu_objects);
	if (add_to_passes)
		set_gpu_objects_data_job(uintptr_t(gpu_objects));

	BuildSceneData_CpuFast::inst->build_scene_data(cubemap_view, skybox_only);

	auto add_objects_to_passes = [&]() {
		CPUSCOPESTART(add_objects_to_passes);
		ZoneScopedN("add_objects_to_passes");
		// ZoneScopedN("LoopObjects");

		MaterialInstance* const debug_transparent_mat = MaterialInstance::load("transparent_debug.mm");
		const bool wants_transparent_debug = debug_transparent_mat && r_debug_transparents.get_bool();
		const bool wants_set_to_fallback = r_force_all_materials_to_fallback.get_bool();
		const bool dont_use_cam_depth = r_dont_use_camera_depth_build_scene.get_bool();
		const bool all_object_depth_prepass = r_depth_prepass_all_objects.get_bool();

		const bool set_transparents_to_default =
			r_debug_mode.get_integer() != 0 && r_debug_mode.get_integer() != gpu::DEBUG_ALBEDO;

		for (int i = 0; i < proxy_list.objects.size(); i++) {
			auto& obj = proxy_list.objects[i];
			handle<Render_Object> objhandle{obj.handle};
			auto& proxy = obj.type_.proxy;

			// go down this path if:
			//		have transparents, not being rendered in fast path
			//		is_skybox (render these last)

			if (!proxy.visible || !proxy.model)
				continue;
			const bool is_in_fastpath =
				BuildSceneData_CpuFast::inst->is_modptr_index_in_fast_path(obj.type_.fastcpu_index) && !proxy.is_skybox;
			const bool has_transparent = obj.type_.has_transparents;
			const bool ed_selected = proxy.outline;
			if (is_in_fastpath && !has_transparent && !ed_selected && !proxy.is_skybox)
				continue;

			// #####################
			// # UNLOADING TESTING #
			// #####################
			// possible for model to not be loaded here. ie user caches a model ptr, not in render system.
			// model is unloaded because its not "used", then user tries using the ptr without going through asset
			// system
			if (!proxy.model->is_valid_to_use()) {
				sys_print(Debug, "emergency model reload %s\n", proxy.model->get_name().c_str());
				g_assets.reload<Model>(proxy.model);
			}
			if (proxy.model->get_num_lods() == 0)
				continue;

			if (!proxy.is_skybox && skybox_only)
				continue;
			if (cubemap_view && proxy.ignore_in_cubemap)
				continue;

			bool is_visible = true;
			int8_t LOD_index = 0;
			//	split_input_lod_arr(lod_to_render_array[i], is_visible, LOD_index);

			//	const bool is_visible = visible_array[i];
			const bool casts_shadow = proxy.shadow_caster; //&& percentage_2 >= 0.001;

			if (!is_visible && !casts_shadow)
				continue;

			// const int LOD_index = (int)lod_to_render_array[i];
			if (LOD_index < 0)
				continue; // not visible

			int16_t cam_depth = 0;
			if (!dont_use_cam_depth)
				cam_depth = camera_depth_array[i];

			auto model = proxy.model;
			const auto& lod = model->get_lod(LOD_index);

			const int pstart = lod.part_ofs;
			const int pend = pstart + lod.part_count;
			//
			for (int j = pstart; j < pend; j++) {
				auto& part = proxy.model->get_part(j);
				if (is_in_fastpath && !part.is_material_transparent() && !ed_selected)
					continue;

				const MaterialInstance* mat = proxy.model->get_material_for_part(part);
				if (obj.type_.proxy.mat_override)
					mat = (MaterialInstance*)obj.type_.proxy.mat_override;
				if (wants_set_to_fallback || !mat || !mat->is_valid_to_use() ||
					!mat->get_master_material()->is_compilied_shader_valid)
					mat = matman.get_fallback();
				if (set_transparents_to_default && mat->get_master_material()->render_in_forward_pass())
					mat = matman.get_fallback();

				const MasterMaterialImpl* mm = mat->get_master_material();

				auto add_to_pass = [&](Render_Pass& pass) {
					pass.add_object(proxy, objhandle, mat, cam_depth, j, LOD_index, 0, build_for_editor);
				};

				if (mm->render_in_forward_pass()) {
					if (is_visible)
						add_to_pass(transparent_pass);
					// if (!mm->is_translucent() && casts_shadow)
					//	add_to_pass(shadow_pass);
				} else if (!is_in_fastpath) {
					if (casts_shadow)
						add_to_pass(shadow_pass);
					if (is_visible) {
						add_to_pass(gbuffer_pass);
					}
				}

#ifdef EDITOR_BUILD
				if (ed_selected && is_visible) {
					add_to_pass(editor_sel_pass);
				}
#endif
			}
		}
	};
	if (add_to_passes)
		add_objects_to_passes();

	auto make_batches_for_passes = [&]() {
		ZoneScopedN("make_batches_for_passes");

		gbuffer_pass.make_batches(*this);
		shadow_pass.make_batches(*this);
		transparent_pass.make_batches(*this);
		editor_sel_pass.make_batches(*this);

		if (add_to_passes) {
			{
				ZoneScopedN("UploadGpuData");
				// glNamedBufferData(gpu_render_instance_buffer, sizeof(gpu::Object_Instance) * num_ren_objs,
				// gpu_objects, GL_DYNAMIC_DRAW);

				glNamedBufferData(gpu_instance_buffer->get_internal_handle(), num_ren_objs * 64, gpu_objects,
								  GL_DYNAMIC_DRAW);
			}
		}
	};
	make_batches_for_passes();

	auto make_render_lists = [&]() {
		ZoneScopedN("make_render_lists");

		auto make_shadow_render_lists = [&]() {
			JobDecl shadowlistdecl[CascadeShadowMapSystem::CASCADES_USED];
			MakeShadowRenderListParam params[CascadeShadowMapSystem::CASCADES_USED];
			for (int i = 0; i < CascadeShadowMapSystem::CASCADES_USED; i++) {
				params[i].visarray = nullptr;
				params[i].index = i;
				shadowlistdecl[i].func = make_shadow_render_list_job;
				shadowlistdecl[i].funcarg = uintptr_t(&params[i]);
			}
			for (int i = 0; i < CascadeShadowMapSystem::CASCADES_USED; i++) {
				shadowlistdecl[i].func(shadowlistdecl[i].funcarg);
			}
		};
		make_shadow_render_lists();

		build_standard_cpu(gbuffer_rlist, gbuffer_pass, proxy_list);

		build_standard_cpu(transparent_rlist, transparent_pass, proxy_list);
		build_standard_cpu(editor_sel_rlist, editor_sel_pass, proxy_list);
	};
	make_render_lists();

	memArena.free_bottom();
}
