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
