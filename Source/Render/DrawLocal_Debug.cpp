#include "DrawLocal.h"
#include "Framework/Util.h"
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

static void get_view_mat(int idx, glm::vec3 pos, glm::mat4& view, glm::vec3& front) {
	vec3 up = vec3(0, -1, 0);
	switch (idx) {
	case 0:
		front = vec3(1, 0, 0);
		break;
	case 1:
		front = vec3(-1, 0, 0);
		break;
	case 2:
		front = vec3(0, 1, 0);
		up = vec3(0, 0, 1);
		break;
	case 3:
		front = vec3(0, -1, 0);
		up = vec3(0, 0, -1);
		break;
	case 4:
		front = vec3(0, 0, 1);
		break;
	case 5:
		front = vec3(0, 0, -1);
		break;
	}
	view = glm::lookAt(pos, pos + front, up);
}

RSunInternal* Render_Scene::get_main_directional_light() {
	if (!suns.empty())
		return &suns.at(suns.size() - 1);
	return nullptr;
}
Render_Scene::~Render_Scene() {}

void Renderer::on_level_end() {}
void Renderer::on_level_start() {
	disable_taa_this_frame = true;
}

ConfigVar r_disable_animated_velocity_vector("r.disable_animated_velocity_vector", "0", CVAR_BOOL | CVAR_DEV, "");

ConfigVar debug_out_layer("debug_out_layer", "0", CVAR_INTEGER | CVAR_UNBOUNDED, "");
void DebuggingTextureOutput::draw_out() {
	if (!output_tex)
		return;
	if (!output_tex->gpu_ptr) {
		sys_print(Error, "DebuggingTextureOutput has invalid texture\n");
		output_tex = nullptr;
		return;
	}

	auto& device = draw.get_device();

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.blend = BlendState::BLEND;
	state.depth_testing = false;
	state.depth_writes = false;
	state.backface_culling = false;
	using gtt = GraphicsTextureType;
	auto type = output_tex->gpu_ptr->get_texture_type();

	if (type == gtt::t2D)
		state.program = (draw.prog.tex_debug_2d);
	else if (type == gtt::t2DArray)
		state.program = (draw.prog.tex_debug_2d_array);
	else if (type == gtt::tCubemap)
		state.program = (draw.prog.tex_debug_cubemap);
	else if (type == gtt::tCubemapArray)
		state.program = (draw.prog.tex_debug_cubemap_array);
	else {
		sys_print(Error, "can only debug 2d and 2d array textures\n");
		output_tex = nullptr;
		return;
	}
	const auto size_img = output_tex->gpu_ptr->get_size();
	const int w = size_img.x;
	const int h = size_img.y;

	const float cur_w = draw.get_current_frame_vs().width;
	const float cur_h = draw.get_current_frame_vs().height;

	device.set_pipeline(state);

	draw.shader().set_mat4("Model", mat4(1));
	glm::mat4 proj = glm::ortho(0.f, cur_w, cur_h, 0.f);
	draw.shader().set_mat4("ViewProj", proj);

	draw.shader().set_float("alpha", alpha);
	draw.shader().set_float("mip_slice", mip);

	draw.bind_texture_ptr(0, output_tex->gpu_ptr);

	glm::vec2 upper_left = glm::vec2(0, 1);
	glm::vec2 size = glm::vec2(1, -1);

	MeshBuilderDD dd;
	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(glm::vec2(0, 0), glm::vec2(w * scale, h * scale), upper_left, size, {});
	mb.End();
	dd.init_from(mb);

	dd.draw(MeshBuilderDD::TRIANGLES);
	dd.free();
}
#ifdef EDITOR_BUILD
float Renderer::get_scene_depth_for_editor(int x, int y) {
	ASSERT(!eng->get_is_in_overlapped_period());
	// super slow garbage functions obviously

	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h) {
		sys_print(Error, "invalid mouse coords for mouse_pick_scene\n");
		return {-1};
	}

	gfx().wait_for_gpu_idle();

	const size_t size = cur_h * cur_w;
	float* buffer_pixels = new float[size];

	gfx().download_texture_2d(tex.scene_depth, 0, buffer_pixels, int(size * sizeof(float)));

	y = cur_h - y - 1;

	const size_t ofs = cur_w * y + x;
	const float depth = buffer_pixels[ofs];
	delete[] buffer_pixels;

	return -current_frame_view.near / depth; // linearize_depth(depth, vs.near, vs.far);
}

handle<Render_Object> Renderer::mouse_pick_scene_for_editor(int x, int y) {
	auto handles = mouse_box_select_for_editor(x, y, 1, 1);
	if (handles.empty())
		return {-1};
	return handles.at(0);
}

std::vector<handle<Render_Object>> Renderer::mouse_box_select_for_editor(int x, int y, int w, int h) {
	assert(!eng->get_is_in_overlapped_period());
	sys_print(Debug, "Renderer::mouse_box_select_for_editor\n");
	assert(w >= 0 && h >= 0);
	// super DUPER slow garbage functions obviously
	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h || x + w >= cur_w || y + h >= cur_h) {
		sys_print(Error, "Renderer::mouse_box_select_for_editor: invalid mouse coords\n");
		return {};
	}
	gfx().wait_for_gpu_idle();
	const int size = cur_h * cur_w * 4;
	std::vector<uint8_t> bufferPixels(size, 0);
	gfx().download_texture_2d(tex.editor_id_buffer, 0, bufferPixels.data(), size);
	y = cur_h - y - 1;
	std::unordered_set<int> found;
	const int skip_pixels = 4; // check every 4 pixels
	for (int xCoordOfs = 0; xCoordOfs < w; xCoordOfs += skip_pixels) {
		for (int yCoordOfs = 0; yCoordOfs < h; yCoordOfs += skip_pixels) {
			const int xCoord = x + xCoordOfs;
			const int yCoord = y - yCoordOfs;
			assert(yCoord >= 0);
			const int ofs = cur_w * yCoord * 4 + xCoord * 4;
			assert(ofs >= 0 && ofs < (int)bufferPixels.size());
			uint8_t* ptr = &bufferPixels.at(ofs);
			uint32_t id = uint32_t(ptr[0]) | uint32_t(ptr[1]) << 8 | uint32_t(ptr[2]) << 16 | uint32_t(ptr[3]) << 24;
			if (id != 0xff000000) {
				uint32_t realid = id - 1; // allow for nullptr
				if (realid < scene.proxy_list.objects.size()) {
					int handle_out = scene.proxy_list.objects.at(realid).handle;
					found.insert(handle_out);
				}
			}
		}
	}

	std::vector<handle<Render_Object>> outObjs;
	for (int f : found)
		outObjs.push_back(handle<Render_Object>{f});
	return outObjs;
}
#endif

// CheckGlErrorInternal_ moved to OpenGlDevice.cpp (Phase 1.1 wrap).