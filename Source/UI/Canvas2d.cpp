#include "UI/Canvas2d.h"
#include "Render/Canvas2dRecorder.h"
#include "Render/Canvas2dVertexArray.h"
#include "Render/Canvas2dTypes.h"
#include "Render/Canvas2dBackendLocal.h"
#include "Render/DrawLocal.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "UI/UILoader.h" // GuiFont -- renamed FontAsset in a later build-order step
#include "UI/GUISystemPublic.h" // UiSystem::inst -- removed once ViewportSystem lands (build-order step 6)
#include "Assets/AssetDatabase.h"
#include "GameEnginePublic.h"
#include <glm/gtc/matrix_transform.hpp>
#include <utility>
#include <SDL3/SDL.h>

namespace {
Canvas2dRecorder g_recorder;
const MaterialInstance* g_default_ui_mat = nullptr;
const MaterialInstance* g_default_font_mat = nullptr;
const GuiFont* g_default_font = nullptr;
IGraphicsTexture* g_depth_texture = nullptr;

DrawSettings make_default_settings(BlendState blend, float z) {
	DrawSettings s;
	s.custom_shader = g_default_ui_mat;
	s.blend = blend;
	s.z = z;
	return s;
}
} // namespace

void Canvas2d::init() {
	g_default_ui_mat = g_assets.find<MaterialInstance>("eng/uiDefault.mm").get();
	if (!g_default_ui_mat)
		Fatalf("Couldnt find default ui material");
	g_default_font = g_assets.find<GuiFont>("eng/sengo24.fnt");
	if (!g_default_font)
		Fatalf("couldnt load default font");
	g_default_font_mat = g_assets.find<MaterialInstance>("eng/fontDefault.mm");
	if (!g_default_font_mat)
		Fatalf("couldnt load default font material");

	auto sz = draw.get_ui_composite_size();
	on_resize(glm::max(sz.x, 1), glm::max(sz.y, 1));
}

void Canvas2d::on_resize(int w, int h) {
	if (g_depth_texture)
		safe_release(g_depth_texture);
	CreateTextureArgs args;
	args.type = GraphicsTextureType::t2D;
	args.format = GraphicsTextureFormat::depth24f;
	args.width = w;
	args.height = h;
	args.num_mip_maps = 1;
	args.sampler_type = GraphicsSamplerType::NearestClamped;
	g_depth_texture = gfx().create_texture(args);
}

void Canvas2d::update() {
	auto sz = draw.get_ui_composite_size();
	static glm::ivec2 last_size{0, 0};
	if (sz != last_size && sz.x > 0 && sz.y > 0) {
		on_resize(sz.x, sz.y);
		last_size = sz;
	}

	g_recorder.begin_frame();
	set_target_window();
}

void Canvas2d::sync_to_renderer() {
	g_recorder.get_transient_gpu_mesh().upload_from(g_recorder.get_transient_arena());
	draw.get_canvas2d_drawer()->set_depth_texture(g_depth_texture, draw.get_ui_composite_target());
	draw.get_canvas2d_drawer()->update(std::move(g_recorder.get_batches()));
}

void Canvas2d::set_target_window() {
	auto sz = draw.get_ui_composite_size();
	g_recorder.set_target(draw.get_ui_composite_target(), sz);
}

void Canvas2d::set_target_texture(Texture* render_texture) {
	ASSERT(render_texture);
	g_recorder.set_target(render_texture->gpu_ptr, render_texture->get_size());
}

void Canvas2d::set_clear(bool clear_color, lColor color, bool clear_depth) {
	g_recorder.set_clear(clear_color, color.to_color32(), clear_depth);
}

void Canvas2d::set_viewport(int x, int y, int w, int h) { g_recorder.set_viewport(Rect2d(x, y, w, h)); }

void Canvas2d::set_view_matrix(glm::mat4 view) { g_recorder.set_view_matrix(view); }

void Canvas2d::set_scissor(int x, int y, int w, int h) { g_recorder.set_scissor(true, Rect2d(x, y, w, h)); }

void Canvas2d::clear_scissor() { g_recorder.set_scissor(false, Rect2d()); }

void Canvas2d::push_scissor(int x, int y, int w, int h) { g_recorder.push_scissor(Rect2d(x, y, w, h)); }

void Canvas2d::pop_scissor() { g_recorder.pop_scissor(); }

void Canvas2d::draw_sprite(float x, float y, float w, float h, Texture* tex, lColor color) {
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	settings.texture = tex;
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_sprite(g_recorder.get_transient_arena(), glm::vec2(x, y), glm::vec2(w, h), glm::vec2(0, 1),
									glm::vec2(1, -1), color.to_color32(), 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::draw_sprite_ex(float x, float y, float w, float h, Texture* tex, lColor color, lRect uv_rect,
							   BlendState blend, float z, glm::mat4 transform) {
	DrawSettings settings = make_default_settings(blend, z);
	settings.texture = tex;
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_sprite_transformed(g_recorder.get_transient_arena(), glm::vec2(w, h),
												glm::vec2(uv_rect.x, uv_rect.y), glm::vec2(uv_rect.w, uv_rect.h),
												color.to_color32(),
												glm::translate(glm::mat4(1.f), glm::vec3(x, y, 0.f)) * transform, z);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::draw_text(std::string text, int x, int y, lColor color, GuiFont* font, guiAnchor anchor) {
	if (!font)
		font = (GuiFont*)g_default_font;
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	settings.custom_shader = g_default_font_mat;
	settings.texture = font->font_texture.get();
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_text(g_recorder.get_transient_arena(), text, glm::vec2(x, y), font, color.to_color32(),
								  anchor, 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

lRect Canvas2d::measure_text(std::string text, GuiFont* font) {
	if (!font)
		font = (GuiFont*)g_default_font;
	int x = 0;
	int y = -font->base;
	for (char c : text) {
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end())
			x += 10;
		else
			x += find->second.advance;
	}
	return lRect(Rect2d(0, y, x, font->lineHeight));
}

lRect Canvas2d::get_screen_size() {
	auto sz = draw.get_ui_composite_size();
	return lRect(Rect2d(0, 0, sz.x, sz.y));
}

lRect Canvas2d::get_target_size() {
	auto sz = g_recorder.get_target_size();
	return lRect(Rect2d(0, 0, sz.x, sz.y));
}

void Canvas2d::rectangle(int x, int y, int w, int h, lColor color) {
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_sprite(g_recorder.get_transient_arena(), glm::vec2(x, y), glm::vec2(w, h), glm::vec2(0, 1),
									glm::vec2(1, -1), color.to_color32(), 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::rectangle_outline(int x, int y, int w, int h, int thickness, lColor color) {
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_rect_outline(g_recorder.get_transient_arena(), glm::vec2(x, y), glm::vec2(w, h),
										  (float)thickness, color.to_color32(), 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::circle(int x, int y, int radius, int segments, lColor color) {
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_circle(g_recorder.get_transient_arena(), glm::vec2(x, y), (float)radius, segments,
									color.to_color32(), 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::line(int x1, int y1, int x2, int y2, int thickness, lColor color) {
	DrawSettings settings = make_default_settings(BlendState::BLEND, 0.f);
	int start = (int)g_recorder.get_transient_arena().get_i().size();
	Canvas2dGeometry::build_line(g_recorder.get_transient_arena(), glm::vec2(x1, y1), glm::vec2(x2, y2),
								 (float)thickness, color.to_color32(), 0.f);
	int count = (int)g_recorder.get_transient_arena().get_i().size() - start;
	g_recorder.submit(&g_recorder.get_transient_gpu_mesh(), start, count, settings, glm::mat4(1.f), false);
}

void Canvas2d::draw_vertex_array(Canvas2dVertexArray& arr, int index_offset, int index_count,
								  const DrawSettings& settings, glm::mat4 transform) {
	g_recorder.submit(&arr.gpu_mesh, index_offset, index_count, settings, transform, true);
}

void Canvas2d::set_window_fullscreen(bool is_fullscreen) { SDL_SetWindowFullscreen(eng->get_os_window(), is_fullscreen); }

void Canvas2d::set_window_title(std::string name) { SDL_SetWindowTitle(eng->get_os_window(), name.c_str()); }

void Canvas2d::set_window_capture_mouse(bool capturing_mouse) {
	UiSystem::inst->set_game_capture_mouse(capturing_mouse);
}

const GuiFont* Canvas2d::get_default_font() { return g_default_font; }
