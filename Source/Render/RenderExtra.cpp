#include "RenderExtra.h"
#include "Render/DrawLocal.h"
#include "Frustum.h"
#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

ConfigVar shadow_map_quality("r.shadow_map_quality", "0", CVAR_INTEGER, "", 0, 1);
ConfigVar r_shadows("r.shadows", "1", CVAR_BOOL, "");

ShadowMapAtlas::ShadowMapAtlas() {
	ASSERT(gfx_is_initialized());

	int size = 1024;
	if (shadow_map_quality.get_integer())
		size = 2048;
	atlas_size = {size, size};
	vtsHandle = Texture::install_system("_spto_shadow");

	CreateTextureArgs args;
	args.width = size;
	args.height = size;
	args.num_mip_maps = 1;
	args.format = GraphicsTextureFormat::depth16f;
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::AtlasShadowmap;
	safe_release(atlas);
	atlas = gfx().create_texture(args);

	vtsHandle->update_specs_ptr(atlas);

	// add the rects
	const int subCount = 4;
	const int subSize = size / subCount; // 256 or 512
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
	ASSERT(size >= 0);
	for (int i = 0; i < (int)rects.size(); i++) {
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
		assert(handle >= 0 && handle < (int)rects.size());
		assert(rects.at(handle).used);
		rects.at(handle).used = false;
	}
}

Rect2d ShadowMapAtlas::get_atlas_rect(int handle) {
	ASSERT(handle != -1);
	return rects.at(handle).rect;
}

IGraphicsTexture* ShadowMapAtlas::get_atlas_texture() {
	ASSERT(atlas != nullptr);
	return atlas;
}

ShadowMapManager::ShadowMapManager() {
	ASSERT(gfx_is_initialized());
	CreateBufferArgs args;
	args.size = sizeof(gpu::Ubo_View_Constants_Struct);
	args.flags = BUFFER_USE_DYNAMIC;
	frame_view = gfx().create_buffer(args);
}

// some stuff to do:
//	lights given priority based on dist to camera and size
//  if atlas is full start evicting and pick highest priority
//  static updated only once
//  only update if in frustum obv

struct LightSortObj
{
	RL_Internal* light{};
	float score = 0.0;
};

void ShadowMapManager::update() {
	ASSERT(gfx_is_initialized());

	auto& memarena = draw.get_arena();
	auto& lights = draw.scene.light_list.objects;
	ArenaScope scope(memarena);
	arena_vec<LightSortObj> shadowlights(scope);
	shadowlights.reserve(lights.size());

	auto& vs = draw.get_current_frame_vs();

	for (auto& [_, l] : lights) {
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
		} else {
			if (l.shadow_array_handle != -1) {
				atlas.free(l.shadow_array_handle);
				l.shadow_array_handle = -1;
			}
		}
	}
	std::sort(shadowlights.begin(), shadowlights.end(),
			  [](LightSortObj& a, LightSortObj& b) { return a.score > b.score; });

	// keep highest
	const int totalrects = atlas.total_rects();
	for (int i = totalrects; i < (int)shadowlights.size(); i++) {
		if (shadowlights[i].light->shadow_array_handle != -1) {
			atlas.free(shadowlights[i].light->shadow_array_handle);
			shadowlights[i].light->shadow_array_handle = -1;
		}
	}
	for (int i = 0; i < totalrects && i < (int)shadowlights.size(); i++) {
		if (shadowlights[i].light->shadow_array_handle == -1) {
			shadowlights[i].light->shadow_array_handle = atlas.allocate(0);
			if (shadowlights[i].light->light.casts_shadow_mode == 2)
				shadowlights[i].light->updated_this_frame = true;
		}
	}
}

void ShadowMapManager::get_lights_to_render(std::vector<handle<Render_Light>>& vec) {
	ASSERT(gfx_is_initialized());

	// if light is static and was updated, render
	// if light is dynamic and was updated or had dynamic in frustum last update, render
	// else skip
	Frustum frustum;
	build_a_frustum_for_perspective(frustum, draw.get_current_frame_vs(), {});

	auto is_in_frustum = [&](RL_Internal& l) -> bool {
		glm::vec3 center = glm::vec3(l.light.position + l.light.normal * l.light.radius * 0.5f);
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
		if (is_alloced && casts_shadow) {
			// always update static
			if ((l.light.casts_shadow_mode == 2 && was_updated))
				vec.push_back({handle});
			else if (l.light.casts_shadow_mode == 1) {
				if (is_in_frustum(l)) {
					vec.push_back({handle});
				}
			}
		}
		l.updated_this_frame = false;
	}
}

void cull_and_draw_spot(Frustum f);
extern ConfigVar r_spot_near;
void ShadowMapManager::do_render(Render_Lists& list, handle<Render_Light> handle, bool any_dynamic_in_frustum) {
	ASSERT(gfx_is_initialized());

	if (!r_shadows.get_bool())
		return;

	// render, update dynamic in frustum flag
	auto& light = draw.scene.light_list.get(handle.id);

	{
		auto& device = draw.get_device();

		RenderPassState pass_setup;
		pass_setup.depth_info = atlas.get_atlas_texture();
		gfx().set_render_pass(pass_setup);

		assert(light.shadow_array_handle != -1);
		Rect2d rect = atlas.get_atlas_rect(light.shadow_array_handle);
		device.set_viewport(rect.x, rect.y, rect.w, rect.h);
		glEnable(GL_SCISSOR_TEST);
		glScissor(rect.x, rect.y, rect.w, rect.h);
		device.clear_framebuffer(true, true, 0.f /* depth value of 0.f to clear*/);
		glDisable(GL_SCISSOR_TEST);

		View_Setup viewSetup;
		viewSetup.width = rect.w;
		viewSetup.height = rect.h;
		viewSetup.near = r_spot_near.get_float();
		viewSetup.far = light.light.radius;
		viewSetup.viewproj = light.lightViewProj;

		Render_Level_Params params(viewSetup, &list, &draw.scene.shadow_pass, Render_Level_Params::SHADOWMAP);

		params.provied_constant_buffer = frame_view;
		params.upload_constants = true;
		params.offset_poly_units = -3;
		draw.render_level_to_target(params);

		cull_and_draw_spot(build_frustum_for_light(light));
	}
}

void ShadowMapManager::on_remove_light(handle<Render_Light> h) {
	ASSERT(h.id >= 0);
	auto& light = draw.scene.light_list.get(h.id);
	if (light.shadow_array_handle != -1) {
		atlas.free(light.shadow_array_handle);
		light.shadow_array_handle = -1;
	}
}
