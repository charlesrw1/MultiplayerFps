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
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
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

	float mult_z = 1.0;
	if (override_mat)
		mult_z = 0.6;
	glm::vec4 sphere = model->get_bounding_sphere();
	const float fov_rad = glm::radians(thumbnail_fov.get_float());
	glm::vec3 center = glm::vec3(sphere);
	const float c_mult = 2.0 / fov_rad;
	glm::vec3 cam_pos = center + glm::normalize(glm::vec3(1, 1, 1)) * sphere.w * c_mult * mult_z;
	View_Setup viewSetup = View_Setup(glm::lookAt(cam_pos, center, glm::vec3(0, 1, 0)), fov_rad, 0.01, 100.0, w, h);

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