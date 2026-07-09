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

	const int w = size;
	const int h = size;
	// RenderPassSetup setup("thumbnail", this->fbo, true, true, 0, 0, w, h);
	// auto scope = draw.get_device().start_render_pass(setup);
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

	// Compute view target and effective radius.
	// Models: use AABB center + half-diagonal — tighter and better-centered than bounding sphere
	//         for asymmetric meshes (characters, props with uneven extents).
	// Materials: use AABB min half-extent as the visual sphere radius.
	//   get_bounding_sphere() uses bounds_to_sphere() which sets radius = length(half_extents),
	//   the AABB half-diagonal. For sphere.cmdl that's R*sqrt(3) — camera ends up ~1.73x too
	//   far, filling only ~58% of the image. Min half-extent = actual sphere mesh radius.
	glm::vec3 center;
	float radius;
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
	}

	// Camera: 45° horizontal + ~40° elevation, close to Unreal's thumbnail angle.
	// sin(fov/2) is the correct inscribed-sphere formula. margin=1.0 for materials makes the
	// sphere silhouette exactly tangent to the frustum edges (fills frame edge-to-edge).
	// Models get a small 5% margin since their AABB half-diagonal is an irregular shape.
	const glm::vec3 cam_dir = glm::normalize(glm::vec3(1.0f, 0.85f, 1.0f));
	const float margin = override_mat ? 1.0f : 1.05f;
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