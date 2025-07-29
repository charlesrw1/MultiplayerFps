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
	vtsHandle->type = Texture_Type::TEXTYPE_2D;

	CreateTextureArgs args;
	args.width = size;
	args.height = size;
	args.num_mip_maps = 1;
	args.format = GraphicsTextureFormat::depth32f;
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::AtlasShadowmap;
	if (atlas)
		atlas->release();
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

	vtsHandle->update_specs_ptr(atlas, size, size, 1, {});

	// add the rects
	const int subCount = 4;
	const int subSize = size / 4;	// 256 or 512
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

texhandle ShadowMapAtlas::get_atlas_texture() {
	return atlas->get_internal_handle();
}

ShadowMapManager::ShadowMapManager()
{
	glCreateFramebuffers(1, &shadow_fbo);
	glCreateBuffers(1, &frame_view);
}

void ShadowMapManager::update()
{
	auto& lights = draw.scene.light_list.objects;
	for (auto&[_,l] : lights) {
		const bool casts_shadow = l.light.casts_shadow_mode != 0;
		if (casts_shadow && l.shadow_array_handle == -1) {
			l.shadow_array_handle = atlas.allocate(0);
		}
		else if (!casts_shadow && l.shadow_array_handle != -1) {
			atlas.free(l.shadow_array_handle);
			l.shadow_array_handle = -1;
		}
	}
}

void ShadowMapManager::get_lights_to_render(std::vector<handle<Render_Light>>& vec)
{
	// if light is static and was updated, render
	// if light is dynamic and was updated or had dynamic in frustum last update, render
	// else skip

	auto& lights = draw.scene.light_list.objects;
	for (auto& [handle, l] : lights) {
		const bool casts_shadow = l.light.casts_shadow_mode != 0;
		assert(casts_shadow == (l.shadow_array_handle != -1));

		const bool was_updated = l.updated_this_frame;
		if (casts_shadow) {
			if((l.light.casts_shadow_mode==2&&was_updated) || l.light.casts_shadow_mode==1)
				vec.push_back({ handle });
		}
		l.updated_this_frame = false;
	}


}
extern ConfigVar r_spot_near;
void ShadowMapManager::do_render(Render_Lists& list, handle<Render_Light> handle, bool any_dynamic_in_frustum)
{
	if (!r_shadows.get_bool())
		return;

	// render, update dynamic in furstum flag
	auto& light = draw.scene.light_list.get(handle.id);
	
	{
		auto& device = draw.get_device();
		RenderPassSetup setup("shadowmap", shadow_fbo, false, false /* clear it below */, 0, 0, 100, 100/* dummy vals*/);
		auto scope = device.start_render_pass(setup);
		glNamedFramebufferTexture(shadow_fbo, GL_DEPTH_ATTACHMENT, atlas.get_atlas_texture(), 0);

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
