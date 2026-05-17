#include "RenderExtra.h"
#include "Render/DrawLocal.h"
#include "RectPackerUtil.h"

LightCookieAtlas* LightCookieAtlas::inst = nullptr;
LightCookieAtlas::LightCookieAtlas() {
	ASSERT(ATLAS_WIDTH > 0);
	Texture::install_system("_cookieatlas");
}

void blit_texture_into_thing_because_reasons(IGraphicsTexture* srct, IGraphicsTexture* destt, Rect2d dest) {
	ASSERT(srct != nullptr);
	ASSERT(destt != nullptr);

	auto& device = draw.get_device();

	device.set_viewport(dest.x, dest.y, dest.w, dest.h);

	RenderPipelineState state;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	state.vao = draw.get_empty_vao();
	state.program = draw.get_prog_man().get_obj(draw.prog.fullscreen_draw_texture);
	device.set_pipeline(state);
	device.get_active_shader()->set_ivec2("viewport_size", glm::ivec2(dest.w, dest.h));

	device.bind_texture(0, srct);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void LightCookieAtlas::update() {
	ASSERT(LightCookieAtlas::inst != nullptr);

	auto& lights = draw.scene.light_list.objects;
	bool has_new = false;
	for (auto& [_, l] : lights) {
		if (!l.light.projected_texture || !l.light.projected_texture->gpu_ptr)
			continue;
		if (!MapUtil::contains(rects, l.light.projected_texture)) {
			has_new = true;
			rects.insert({l.light.projected_texture, {}});
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
			if (size.y > 1) { // non ies
				where.push_back(t);
				outrects.push_back(Rect2d(0, 0, size.x, size.y));
			} else {
				num_ies += 1;
				ies_textures.push_back(t);
			}
		}
		const int ies_width = 512;
		outrects.push_back(Rect2d(0, 0, ies_width, num_ies * 3));
		const auto [pos, height] = RectPackerUtil::shelf_pack(outrects, ATLAS_WIDTH);
		for (int i = 0; i < (int)where.size(); i++) {
			auto size = where[i]->gpu_ptr->get_size();
			size = glm::min(size, glm::ivec2(ATLAS_WIDTH));
			rects[where[i]] = Rect2d(pos[i].x, pos[i].y, size.x, size.y);
		}
		for (int i = 0; i < (int)ies_textures.size(); i++) {
			rects[ies_textures[i]] = Rect2d(pos.back().x, pos.back().y + i * 3, ies_width, 3);
		}

		if (atlas)
			atlas->release();
		CreateTextureArgs args;
		args.format = GraphicsTextureFormat::r8;
		args.num_mip_maps = 1;
		args.sampler_type = GraphicsSamplerType::LinearClamped;
		args.type = GraphicsTextureType::t2D;
		args.width = ATLAS_WIDTH;
		args.height = height;
		atlas = gfx().create_texture(args);
		atlasheight = height;

		Texture::load("_cookieatlas")->update_specs_ptr(atlas);

		RenderPassState setup;
		ColorTargetInfo target(atlas);
		target.wants_clear = true; // clear to black (default)
		auto colorinfos = {target};
		setup.color_infos = colorinfos;
		gfx().set_render_pass(setup);

		for (auto& [t, r] : rects) {
			blit_texture_into_thing_because_reasons(t->gpu_ptr, atlas, r);
		}
	}
	for (auto& [_, l] : lights) {
		if (!l.light.projected_texture || !l.light.projected_texture->gpu_ptr)
			continue;
		l.cookie_atlas = get_rect_for_cookie(l.light.projected_texture);
	}
}

glm::vec4 LightCookieAtlas::get_rect_for_cookie(Texture* t) {
	ASSERT(t != nullptr);
	ASSERT(atlasheight > 0);
	Rect2d r = rects[t];
	return glm::vec4((r.x + 0.5001) / float(ATLAS_WIDTH), (r.y + 0.5) / float(atlasheight),
					 (r.w - 1) / float(ATLAS_WIDTH), (r.h - 1.001) / float(atlasheight));
}
