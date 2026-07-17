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
#include <SDL3/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>
#ifdef EDITOR_BUILD
int write_png_wrapper(const char* filename, int w, int h, int comp, const void* data, int stride_in_bytes);

ThumbnailRenderer::ThumbnailRenderer(int size) : pass(pass_type::TRANSPARENT) {
	this->size = size;
	pass.forced_forward = true;
	list.init(0, 0);
	object = draw.scene.register_obj();
	Render_Object o;
	o.visible = false;
	draw.scene.update_obj(object, o);

	const int w = size;
	const int h = size;

	CreateTextureArgs colorArgs;
	colorArgs.width = w;
	colorArgs.height = h;
	colorArgs.format = GraphicsTextureFormat::rgba8;
	colorArgs.sampler_type = GraphicsSamplerType::NearestClamped;
	this->color = gfx().create_texture(colorArgs);
	CreateTextureArgs depthArgs;
	depthArgs.width = w;
	depthArgs.height = h;
	depthArgs.format = GraphicsTextureFormat::depth32f;
	depthArgs.sampler_type = GraphicsSamplerType::NearestClamped;
	this->depth = gfx().create_texture(depthArgs);

	vts_handle = Texture::install_system("_test_thumbnail");
	vts_handle->update_specs_ptr(this->color);
}

void ThumbnailRenderer::render(Model* model, MaterialInstance* override_mat) {
	ASSERT(!eng->get_is_in_overlapped_period());
	if (!model || model->get_num_lods() == 0)
		return;
	pass.clear();
	auto& lod = model->get_lod(0);
	auto& scene = draw.scene;
	const int pstart = lod.part_ofs;
	const int pend = pstart + lod.part_count;
	auto& proxy = scene.proxy_list.get(object.id);
	proxy.proxy.model = model;
	proxy.proxy.transform = glm::mat4(1.f); // always centered at the origin for this path
	for (int j = pstart; j < pend; j++) {
		auto& part = model->get_part(j);

		const MaterialInstance* mat = model->get_material_for_part(part);
		if (override_mat)
			mat = override_mat;

		if (!mat || !mat->is_valid_to_use() || !mat->get_master_material()->is_compilied_shader_valid)
			mat = matman.get_fallback();

		pass.add_object(proxy.proxy, object, mat, 0, j, 0, 0, false);
	}
	pass.make_batches(scene);
	build_standard_cpu(list, pass, scene.proxy_list);

	// Compute view target and effective radius.
	// Models: use AABB center + half-diagonal — tighter and better-centered than bounding sphere
	//         for asymmetric meshes (characters, props with uneven extents).
	// Materials: use AABB min half-extent as the visual sphere radius.
	//   get_bounding_sphere() uses bounds_to_sphere() which sets radius = length(half_extents),
	//   the AABB half-diagonal. For sphere.cmdl that's R*sqrt(3) — camera ends up ~1.73x too
	//   far, filling only ~58% of the image. Min half-extent = actual sphere mesh radius.
	glm::vec3 center;
	float radius;
	bool tight_margin;
	if (override_mat) {
		const auto& aabb = model->get_bounds();
		center = (aabb.bmin + aabb.bmax) * 0.5f;
		glm::vec3 half_ext = (aabb.bmax - aabb.bmin) * 0.5f;
		radius = glm::min(half_ext.x, glm::min(half_ext.y, half_ext.z));
		if (radius < 1e-3f) {
			glm::vec4 sphere = model->get_bounding_sphere();
			center = glm::vec3(sphere);
			radius = sphere.w;
		}
		tight_margin = true;
	} else {
		const auto& aabb = model->get_bounds();
		center = (aabb.bmin + aabb.bmax) * 0.5f;
		glm::vec3 half_ext = (aabb.bmax - aabb.bmin) * 0.5f;
		radius = glm::length(half_ext);
		if (radius < 1e-3f) {
			// Degenerate AABB — fall back to bounding sphere
			glm::vec4 sphere = model->get_bounding_sphere();
			center = glm::vec3(sphere);
			radius = sphere.w;
		}
		tight_margin = false;
	}

	draw_and_output(center, radius, tight_margin);
}

void ThumbnailRenderer::render_multi(const std::vector<ThumbnailRenderItem>& items) {
	ASSERT(!eng->get_is_in_overlapped_period());
	pass.clear();
	auto& scene = draw.scene;

	// Grow the proxy pool to cover this many items, permanently — the extra slots are
	// cheap and get reused by later, smaller thumbnails. New proxies default to
	// visible=true (Render_Object's default), which would otherwise leak them into the
	// real scene's gbuffer/shadow/transparent passes (Render_Scene::build_scene_data
	// iterates every registered proxy in this shared draw.scene, filtering only on
	// proxy.visible) — force them invisible immediately, same as the ctor does for `object`.
	while (multi_objects.size() < items.size()) {
		auto h = scene.register_obj();
		Render_Object o;
		o.visible = false;
		scene.update_obj(h, o);
		multi_objects.push_back(h);
	}

	Bounds combined;
	size_t used = 0;
	for (const auto& item : items) {
		if (!item.model || item.model->get_num_lods() == 0)
			continue;
		const auto& lod = item.model->get_lod(0);
		const int pstart = lod.part_ofs;
		const int pend = pstart + lod.part_count;

		auto object_handle = multi_objects[used++];
		auto& proxy = scene.proxy_list.get(object_handle.id);
		proxy.proxy.model = item.model;
		proxy.proxy.transform = item.transform;
		for (int j = pstart; j < pend; j++) {
			auto& part = item.model->get_part(j);
			const MaterialInstance* mat = item.model->get_material_for_part(part);
			if (!mat || !mat->is_valid_to_use() || !mat->get_master_material()->is_compilied_shader_valid)
				mat = matman.get_fallback();
			pass.add_object(proxy.proxy, object_handle, mat, 0, j, 0, 0, false);
		}
		combined = bounds_union(combined, transform_bounds(item.transform, item.model->get_bounds()));
	}
	// Hide any pool slots beyond what this render needs — leftovers from a previous,
	// larger thumbnail.
	for (size_t i = used; i < multi_objects.size(); i++) {
		auto& proxy = scene.proxy_list.get(multi_objects[i].id);
		proxy.proxy.model = nullptr;
	}

	// The transforms just written above only exist in proxy_list's CPU-side struct so far.
	// The actual draw call reads per-object transforms from scene.gpu_instance_buffer, which
	// is normally refreshed once per real frame by Render_Scene::build_scene_data() — that
	// happens earlier in the frame (main scene draw), before this thumbnail code runs (editor
	// UI draw), so without an explicit sync here the GPU would still see last frame's stale
	// transform/model count for these proxies (wrong positions, or missing entries entirely
	// for pool slots grown just now) until the next real frame's build_scene_data() catches up.
	scene.sync_gpu_object_transforms();

	pass.make_batches(scene);
	build_standard_cpu(list, pass, scene.proxy_list);

	glm::vec3 center = (combined.bmin + combined.bmax) * 0.5f;
	glm::vec3 half_ext = (combined.bmax - combined.bmin) * 0.5f;
	float radius = glm::length(half_ext);
	if (!(radius > 1e-3f)) {
		center = glm::vec3(0.f);
		radius = 1.f;
	}

	draw_and_output(center, radius, false);
}

void ThumbnailRenderer::draw_and_output(const glm::vec3& center, float radius, bool tight_margin) {
	const int w = size;
	const int h = size;
	auto set_pass = [&]() {
		RenderPassState passState;
		ColorTargetInfo color_target(color);
		color_target.wants_clear = true; // clear to black (default)
		auto color_infos = {color_target};
		passState.color_infos = color_infos;
		passState.depth_info = depth;
		passState.wants_depth_clear = true;
		gfx().set_render_pass(passState);
	};
	set_pass();

	const float fov_rad = glm::radians(thumbnail_fov.get_float());

	// Camera: 45° horizontal + ~40° elevation, close to Unreal's thumbnail angle.
	// sin(fov/2) is the correct inscribed-sphere formula. margin=1.0 makes the bounding
	// sphere silhouette exactly tangent to the frustum edges (fills frame edge-to-edge).
	// Non-tight subjects get a small 5% margin since their AABB half-diagonal is an
	// irregular shape (an actual sphere silhouette, e.g. materials, can use margin=1.0).
	const glm::vec3 cam_dir = glm::normalize(glm::vec3(1.0f, 0.85f, 1.0f));
	const float margin = tight_margin ? 1.0f : 1.05f;
	const float dist = radius / glm::sin(fov_rad * 0.5f) * margin;
	const glm::vec3 cam_pos = center + cam_dir * dist;
	const float far_plane = glm::max(100.0f, dist + radius * 2.0f);
	View_Setup viewSetup = View_Setup(glm::lookAt(cam_pos, center, glm::vec3(0, 1, 0)), fov_rad, 0.01f, far_plane, w, h);

	Render_Level_Params cmdparams(viewSetup, &list, &pass, Render_Level_Params::FORWARD_PASS);
	cmdparams.upload_constants = true;
	cmdparams.provied_constant_buffer = draw.ubo.current_frame;
	cmdparams.draw_viewmodel = true;
	draw.render_level_to_target(cmdparams);
}

void ThumbnailRenderer::output_to_path(std::string path) {
	const int w = size;
	const int h = size;
	std::vector<unsigned char> pixels(w * h * 4); // RGBA
	color->download(0, -1, pixels.data(), (int)pixels.size());
	int success = write_png_wrapper(path.c_str(), w, h, 4, pixels.data(), w * 4);
}
#endif