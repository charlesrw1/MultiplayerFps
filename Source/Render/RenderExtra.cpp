#include "RenderExtra.h"
#include "Render/DrawLocal.h"

ConfigVar shadow_map_quality("r.shadow_map_quality", "0", CVAR_INTEGER, "", 0, 1);
ConfigVar r_shadows("r.shadows", "1", CVAR_BOOL, "");
ShadowMapAtlas::ShadowMapAtlas() {
	int size = 1024;
	if (shadow_map_quality.get_integer())
		size = 2048;
	atlas_size = { size,size };
	vtsHandle = Texture::install_system("_spto_shadow");

	CreateTextureArgs args;
	args.width = size;
	args.height = size;
	args.num_mip_maps = 1;
	args.format = GraphicsTextureFormat::depth16f;
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::AtlasShadowmap;
	safe_release(atlas);
	atlas = IGraphicsDevice::inst->create_texture(args);

	//glDeleteTextures(1, &atlas_texture);
	//glCreateTextures(GL_TEXTURE_2D, 1, &atlas_texture);
	//glTextureStorage2D(atlas_texture, 1, GL_DEPTH_COMPONENT32F, size, size);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	//glTextureParameteri(atlas_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	vtsHandle->update_specs_ptr(atlas);

	// add the rects
	const int subCount = 4;
	const int subSize = size / subCount;	// 256 or 512
	for (int x = 0; x < subCount; x++) {
		for (int y = 0; y < subCount; y++) {
			Rect2d rect;
			rect.w = rect.h = subSize;
			rect.x = x * subSize;
			rect.y = y * subSize;
			Available a;
			a.rect = rect;
			rects.push_back(a);
		}
	}
}

int ShadowMapAtlas::allocate(int8_t size) {
	for (int i = 0; i < rects.size(); i++) {
		if (!rects[i].used) {
			rects[i].used = true;
			return i;
		}
	}
	return -1;
}

void ShadowMapAtlas::free(int handle) {
	if (handle == -1)
		return;
	else {
		assert(handle >= 0 && handle < rects.size());
		assert(rects.at(handle).used);
		rects.at(handle).used = false;
	}
}

Rect2d ShadowMapAtlas::get_atlas_rect(int handle) {
	assert(handle != -1);
	return rects.at(handle).rect;
}

IGraphicsTexture* ShadowMapAtlas::get_atlas_texture() {
	return atlas;
}

ShadowMapManager::ShadowMapManager()
{
	glCreateBuffers(1, &frame_view);
}


// some stuff to do:
//	lights given priority based on dist to camera and size
//  if atlas is full start evicting and pick highest priority
//  static updated only once
//  only update if in frustum obv


#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"
struct LightSortObj {
	RL_Internal* light{};
	float score = 0.0;
};
#include <algorithm>
void ShadowMapManager::update()
{
	auto& memarena = draw.get_arena();
	auto& lights = draw.scene.light_list.objects;
	ArenaScope scope(memarena);
	arena_vec<LightSortObj> shadowlights(scope);
	shadowlights.reserve(lights.size());

	auto& vs = draw.get_current_frame_vs();

	for (auto&[_,l] : lights) {
		const bool casts_shadow = l.light.casts_shadow_mode != 0;
		if (casts_shadow) {
			LightSortObj o;
			o.light = &l;
			auto origin = o.light->light.position;
			float radius = o.light->light.radius;
			if (l.light.is_spotlight) {
				origin += l.light.normal * l.light.radius * 0.5f;
				radius = l.light.radius * 0.5f;
			}

			auto to = origin - vs.origin;
			float dist = glm::dot(to, to);
			o.score = radius / dist;
			shadowlights.push_back(o);
		}
		else {
			if (l.shadow_array_handle != -1) {
				atlas.free(l.shadow_array_handle);
				l.shadow_array_handle = -1;
			}
		}
	}
	std::sort(shadowlights.begin(), shadowlights.end(), [](LightSortObj& a, LightSortObj& b) {
		return a.score > b.score;
		});

	// keep highest
	const int totalrects = atlas.total_rects();
	for (int i = totalrects; i < shadowlights.size(); i++) {
		if (shadowlights[i].light->shadow_array_handle != -1) {
			atlas.free(shadowlights[i].light->shadow_array_handle);
			shadowlights[i].light->shadow_array_handle = -1;
		}
	}
	for (int i = 0; i < totalrects && i < shadowlights.size(); i++) {
		if (shadowlights[i].light->shadow_array_handle == -1) {
			shadowlights[i].light->shadow_array_handle=atlas.allocate(0);
			if (shadowlights[i].light->light.casts_shadow_mode == 2)
				shadowlights[i].light->updated_this_frame = true;
		}
	}
}

void ShadowMapManager::get_lights_to_render(std::vector<handle<Render_Light>>& vec)
{
	// if light is static and was updated, render
	// if light is dynamic and was updated or had dynamic in frustum last update, render
	// else skip
	Frustum frustum;
	build_a_frustum_for_perspective(frustum, draw.get_current_frame_vs(), {});

	auto is_in_frustum = [&](RL_Internal& l)  -> bool {
		glm::vec3 center = glm::vec3(l.light.position+l.light.normal*l.light.radius*0.5f);
		const float radius = l.light.radius * 0.5f;

		int res = 0;
		res += (glm::dot(glm::vec3(frustum.top_plane), center) + frustum.top_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.bot_plane), center) + frustum.bot_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.left_plane), center) + frustum.left_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.right_plane), center) + frustum.right_plane.w >= -radius) ? 1 : 0;

		const bool is_visible = res == 4;

		return is_visible;
	};

	auto& lights = draw.scene.light_list.objects;
	for (auto& [handle, l] : lights) {
		const bool casts_shadow = l.light.casts_shadow_mode != 0;
		const bool is_alloced = l.shadow_array_handle != -1;
		const bool was_updated = l.updated_this_frame;
		if (is_alloced&&casts_shadow ) {
			// always update static
			if ((l.light.casts_shadow_mode == 2 && was_updated))
				vec.push_back({ handle });
			else if (l.light.casts_shadow_mode == 1) {
				if (is_in_frustum(l)) {
					vec.push_back({ handle });
				}
			}
		}
		l.updated_this_frame = false;
	}
}

#include "Frustum.h"
void cull_and_draw_spot(Frustum f);
extern ConfigVar r_spot_near;
void ShadowMapManager::do_render(Render_Lists& list, handle<Render_Light> handle, bool any_dynamic_in_frustum)
{
	if (!r_shadows.get_bool())
		return;

	// render, update dynamic in furstum flag
	auto& light = draw.scene.light_list.get(handle.id);
	
	{
		auto& device = draw.get_device();
		//RenderPassSetup setup("shadowmap", shadow_fbo, false, false /* clear it below */, 0, 0, 100, 100/* dummy vals*/);
		//auto scope = device.start_render_pass(setup);
		//glNamedFramebufferTexture(shadow_fbo, GL_DEPTH_ATTACHMENT, atlas.get_atlas_texture(), 0);
		
		RenderPassState pass_setup;
		pass_setup.depth_info = atlas.get_atlas_texture();
		IGraphicsDevice::inst->set_render_pass(pass_setup);


		assert(light.shadow_array_handle != -1);
		Rect2d rect = atlas.get_atlas_rect(light.shadow_array_handle);
		//rect.w = 512;
		//rect.h = 512;
		device.set_viewport(rect.x,rect.y,rect.w,rect.h);
		glEnable(GL_SCISSOR_TEST);
		glScissor(rect.x, rect.y, rect.w, rect.h);
		device.clear_framebuffer(true, true, 0.f/* depth value of 0.f to clear*/);
		glDisable(GL_SCISSOR_TEST);

		View_Setup viewSetup;
		viewSetup.width = rect.w;
		viewSetup.height = rect.h;
		viewSetup.near = r_spot_near.get_float();
		viewSetup.far = light.light.radius;
		viewSetup.viewproj = light.lightViewProj;
		//viewSetup.view = setup.proj = mat4(1);	// unused

		Render_Level_Params params(
			viewSetup,
			&list,
			&draw.scene.shadow_pass,
			Render_Level_Params::SHADOWMAP
		);

		params.provied_constant_buffer = frame_view;
		params.upload_constants = true;
		params.offset_poly_units = -3;
		draw.render_level_to_target(params);

		cull_and_draw_spot(build_frustum_for_light(light));
	}
}

void ShadowMapManager::on_remove_light(handle<Render_Light> h)
{
	auto& light = draw.scene.light_list.get(h.id);
	if (light.shadow_array_handle != -1) {
		atlas.free(light.shadow_array_handle);
		light.shadow_array_handle = -1;
	}
}
#include "RectPackerUtil.h"
LightCookieAtlas* LightCookieAtlas::inst = nullptr;
LightCookieAtlas::LightCookieAtlas()
{
	Texture::install_system("_cookieatlas");
}

void blit_texture_into_thing_because_reasons(
	IGraphicsTexture* srct, 
	IGraphicsTexture* destt,
	Rect2d dest
)
{
	auto& device = draw.get_device();


	//RenderPassState setup;
	//setup.set_clear_both(false);
	//auto colorinfos = { ColorTargetInfo(destt) };
	//setup.color_infos = colorinfos;
	//IGraphicsDevice::inst->set_render_pass(setup);
	device.set_viewport(dest.x, dest.y, dest.w, dest.h);

	RenderPipelineState state;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	state.vao = draw.get_empty_vao();
	state.program = draw.prog.fullscreen_draw_texture;
	device.set_pipeline(state);
	device.shader().set_ivec2("viewport_size", glm::ivec2(dest.w, dest.h));

	device.bind_texture_ptr(0, srct);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void LightCookieAtlas::update()
{
	auto& lights = draw.scene.light_list.objects;
	bool has_new = false;
	for (auto& [_,l] : lights) {
		if (!l.light.projected_texture||!l.light.projected_texture->gpu_ptr)
			continue;
		if (!MapUtil::contains(rects, l.light.projected_texture)) {
			has_new = true;
			rects.insert({ l.light.projected_texture,{} });
		}
	}
	if (has_new) {
		sys_print(Warning, "updating lightcookieatlas...\n");

		std::vector<Texture*> where;
		std::vector<Rect2d> outrects;
		std::vector<Texture*> ies_textures;
		// save room for ies's
		int num_ies = 0;
		for (auto& [t, r] : rects) {
			auto size = t->gpu_ptr->get_size();
			size = glm::min(size, glm::ivec2(ATLAS_WIDTH));
			if (size.y > 1) {	// non ies
				where.push_back(t);
				outrects.push_back(Rect2d(0, 0, size.x, size.y));
			}
			else {
				num_ies += 1;
				ies_textures.push_back(t);
			}
		}
		const int ies_width = 512;
		outrects.push_back(Rect2d(0, 0, ies_width, num_ies*3));
		const auto[pos,height]=RectPackerUtil::shelf_pack(outrects,ATLAS_WIDTH);
		for (int i = 0; i < where.size(); i++) {
			auto size = where[i]->gpu_ptr->get_size();
			size = glm::min(size, glm::ivec2(ATLAS_WIDTH));
			rects[where[i]] = Rect2d(pos[i].x, pos[i].y, size.x, size.y);
		}
		for (int i = 0; i < ies_textures.size(); i++) {
			rects[ies_textures[i]] = Rect2d(pos.back().x, pos.back().y + i*3, ies_width, 3);
		}

		if(atlas)
			atlas->release();
		CreateTextureArgs args;
		args.format = GraphicsTextureFormat::r8;
		args.num_mip_maps = 1;
		args.sampler_type = GraphicsSamplerType::LinearClamped;
		args.type = GraphicsTextureType::t2D;
		args.width = ATLAS_WIDTH;
		args.height = height;
		atlas = IGraphicsDevice::inst->create_texture(args);
		atlasheight = height;

		Texture::load("_cookieatlas")->update_specs_ptr(atlas);

		RenderPassState setup;
		setup.set_clear_both(true);
		auto colorinfos = { ColorTargetInfo(atlas) };
		setup.color_infos = colorinfos;
		IGraphicsDevice::inst->set_render_pass(setup);
		int index = -1;

		for (auto& [t, r] : rects) {
			blit_texture_into_thing_because_reasons(t->gpu_ptr, atlas,
				r);
		}

	}
	for (auto& [_, l] : lights) {
		if (!l.light.projected_texture || !l.light.projected_texture->gpu_ptr)
			continue;
		l.cookie_atlas = get_rect_for_cookie(l.light.projected_texture);
	}
}

glm::vec4 LightCookieAtlas::get_rect_for_cookie(Texture* t)
{
	Rect2d r = rects[t];
	return glm::vec4((r.x+0.5001) / float(ATLAS_WIDTH), (r.y+0.5) / float(atlasheight), (r.w-1) / float(ATLAS_WIDTH), (r.h-1.001) / float(atlasheight));
}
SSRSystem* SSRSystem::inst = nullptr;
SSRSystem::SSRSystem()
{
	ssr_compute = draw.get_prog_man().create_raster("fullscreenquad.txt","ssr_f.txt");
	hiz_downsample = draw.get_prog_man().create_compute("DepthPyramidC.txt");
	ssr_downsample =draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_downsample.txt");
	ssr_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_upsample.txt");

	Texture::install_system("_depth_pyramid2");
	glGenSamplers(1, &hiz_max_sampler);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiz_max_sampler, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MAX);	// max, takes the closest value to the camera. depth buffer stored in reverse-Z [1,0]
}
void SSRSystem::compute_depth()
{
	GPUSCOPESTART(ssr_compute_depth_pyramid);

	draw.get_device().set_shader(hiz_downsample);
	const int levels = Texture::get_mip_map_count(actual_depth_size.x, actual_depth_size.y);
	int width = actual_depth_size.x;
	int height = actual_depth_size.y;
	glBindSampler(0, hiz_max_sampler);
	for (int level = 0; level < levels; level++) {
		//glBindImageTexture()
		glBindImageTexture(1, depth_pyramid->get_internal_handle(), level, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
		if (level == 0)
			draw.get_device().bind_texture_ptr(0, draw.tex.scene_depth);
		else {
			draw.get_device().bind_texture_ptr(0, depth_pyramid);
		}


		int groups_x = glm::ceil(width / 32.f);
		int groups_y = glm::ceil(height / 32.f);
		draw.shader().set_float("width", width);
		draw.shader().set_float("height", height);
		const int level_to_sample = level == 0 ? 0 : level - 1;
		draw.shader().set_int("level", level_to_sample);


		glDispatchCompute(groups_x, groups_y, 1);


		width /= 2.0;
		height /= 2.0;
		width = glm::max(width, 1);
		height = glm::max(height, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
			GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	glBindSampler(0, 0);
}
void SSRSystem::do_downsample()
{
	const auto& viewsetup = draw.current_frame_view;

	auto& device = draw.get_device();
	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_downsample;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	const int num_mips = 5;
	glm::ivec2 size(viewsetup.width, viewsetup.height);
	glm::vec2 inv_presize = 1.f / glm::vec2(size);
	for (int i =0; i < num_mips; i++) {
		size /= 2.f;
		auto targets = {
		ColorTargetInfo(draw.tex.last_reflection_accum,-1,i)
		};
		RenderPassState rp;
		rp.color_infos = targets;
		IGraphicsDevice::inst->set_render_pass(rp);
		int mip_to_fetch = (i == 0) ? 0 : i - 1;
		device.shader().set_int("mip_level", mip_to_fetch);
		device.shader().set_vec2("myimg_size",size);
		device.shader().set_vec2("inv_prev_size", inv_presize);
		if(i==0)
			device.bind_texture_ptr(0, draw.tex.scene_color);
		else
			device.bind_texture_ptr(0, draw.tex.last_reflection_accum);
		device.set_viewport(0,0,size.x, size.y);
		glDrawArrays(GL_TRIANGLES, 0, 3);


		inv_presize = 1.f / glm::vec2(size);
	}
}static float lod_force = 1.0;
static bool debug_toggle = false;
void SSRSystem::do_upsample()
{
	GPUFUNCTIONSTART;

	const auto& viewsetup = draw.current_frame_view;

	auto& device = draw.get_device();
	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_upsample;
	state.blend = BlendState::ADD;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	glm::ivec2 size(viewsetup.width, viewsetup.height);
	glm::vec2 inv_presize = 1.f / glm::vec2(size);

	device.bind_texture_ptr(0, draw.tex.halfres_ssr);
	device.bind_texture_ptr(1, draw.tex.scene_gbuffer0);
	device.bind_texture_ptr(2, draw.tex.last_scene_color);
	device.bind_texture_ptr(3, draw.tex.scene_gbuffer2);
	device.bind_texture_ptr(4, draw.tex.scene_depth);
	device.bind_texture(5, EnviornmentMapHelper::get().integrator.get_texture());
	device.bind_texture_ptr(6,draw.tex.last_reflection_accum);

	static int frame = 0;
	device.shader().set_int("temporal_frame", (frame++) % 4);
	device.shader().set_bool("debug_toggle", debug_toggle);
//	device.shader().set_vec2("myimg_size", size);
	//device.shader().set_float("lod_force", lod_force);

	auto targets = {
		ColorTargetInfo(draw.tex.scene_color)
	};
	RenderPassState rp;
	rp.color_infos = targets;
	IGraphicsDevice::inst->set_render_pass(rp);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}
#include "imgui.h"
static int max_steps = 40;
static float bias = 0.05;
static float step_size = 0.2;
static float max_dist = 100.0;
static float max_thick = 0.07;

void imgui_menu_ssr() {
	ImGui::InputFloat("bias", &bias);
	ImGui::InputFloat("step_size", &step_size);
	ImGui::InputFloat("max_dist", &max_dist);
	ImGui::InputFloat("max_thick", &max_thick);
	ImGui::InputFloat("lod_force", &lod_force);
	ImGui::Checkbox("debug_toggle", &debug_toggle);
	ImGui::InputInt("max_steps", &max_steps);
}
ADD_TO_DEBUG_MENU(imgui_menu_ssr);
void SSRSystem::execute_compute()
{
	GPUSCOPESTART(ssr_system_execute);

	// compute depthp
	const auto& viewsetup = draw.current_frame_view;
	int v_w = viewsetup.width / 2;
	int v_h = viewsetup.height / 2;
	if (depth_size.x != v_w || depth_size.y != v_h)
		init_depth_pyramid(v_w, v_h);
	//compute_depth();

	// compute ssr
	auto& device = draw.get_device();
	auto targets = {
		ColorTargetInfo(draw.tex.halfres_ssr,-1,0)
	};
	RenderPassState rp;
	rp.color_infos = targets;
	IGraphicsDevice::inst->set_render_pass(rp);

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_compute;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	device.shader().set_int("MAX_STEPS", max_steps);
	device.shader().set_float("max_distance", max_dist);
	device.shader().set_float("bias", bias);
	device.shader().set_float("step_size", step_size);
	device.shader().set_float("max_thickness", max_thick);
	device.shader().set_bool("debug_toggle", debug_toggle);
	auto time = GetTime();
	static int index = 0;
	device.shader().set_float("temporalTime", float(index++));// time - std::round(time / 10.0) * 10.0);


	device.shader().set_mat4("g_proj", viewsetup.proj);
	device.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
	device.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
	device.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
	glBindSampler(3, hiz_max_sampler);
	device.bind_texture_ptr(3, depth_pyramid);
	device.bind_texture_ptr(4, draw.tex.scene_depth);
	device.bind_texture_ptr(5, draw.tex.scene_color);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindSampler(3, 0);

	do_downsample();

	do_upsample();

	//GraphicsBlitInfo b;
	//b.dest.texture = draw.tex.scene_color;
	//b.src.texture = draw.tex.halfres_ssr;
	//b.src.mip = 2;
	//b.filter = GraphicsFilterType::Linear;
	//b.set_width_height_both(viewsetup.width, viewsetup.height);
	//IGraphicsDevice::inst->blit_textures(b);
}
extern uint32_t previousPow2(uint32_t v);
void SSRSystem::init_depth_pyramid(int w, int h)
{
	depth_size = { w,h };
	if (depth_pyramid)
		depth_pyramid->release();

	auto actual_width = previousPow2(w)*2;
	auto actual_height = previousPow2(h)*2;
	actual_depth_size = { actual_width,actual_height };


	CreateTextureArgs args;
	args.num_mip_maps = Texture::get_mip_map_count(actual_width, actual_height);
	args.width = actual_width;
	args.height = actual_height;
	//previousPow2(w*2)
	//args.num_mip_maps = getImageMipLevels(actual_width, actual_height);
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::NearestClamped;
	args.format = GraphicsTextureFormat::r32f;

	depth_pyramid = IGraphicsDevice::inst->create_texture(args);

	auto t = Texture::load("_depth_pyramid2");
	t->update_specs_ptr(depth_pyramid);
}