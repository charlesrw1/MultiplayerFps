#include "GUILocal.h"
#include "Render/DrawLocal.h"	// DrawLocal include!!
#include "glad/glad.h"
#include <glm/gtc/matrix_transform.hpp>

#include "Widgets/Layouts.h"
#include "Widgets/Visuals.h"
#include "Widgets/Interactables.h"
#include "GameEnginePublic.h"

// include

CLASS_IMPL(GuiRootPanel);
CLASS_IMPL(GUIBox);
CLASS_IMPL(GUIFullscreen);
CLASS_IMPL(GUIButton);
CLASS_IMPL(GUIText);
CLASS_IMPL(GUIVerticalBox);
ConfigVar ui_debug_press("ui.debug_press", "0", CVAR_BOOL | CVAR_DEV,"");
ConfigVar ui_draw_text_bbox("ui.draw_text_bbox", "0", CVAR_BOOL | CVAR_DEV,"");

struct UIBuilderImpl
{
	const MaterialInstance* current_mat = nullptr;
	const Texture* current_t = nullptr;	// when using default

	// Ortho matrix of screen
	glm::mat4 ViewProj{};
};

UIBuilder::UIBuilder(GuiSystemLocal* s)
{
	sys = s;
	mb = new MeshBuilder();
	impl = new UIBuilderImpl;
}
UIBuilder::~UIBuilder()
{
	mb->Free();
	delete mb;
	delete impl;
}

void UIBuilder::init_drawing_state()
{
	draw.set_blend_state(blend_state::BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, draw.fbo.composite);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.ubo.current_frame);
	float x = sys->root->ws_position.x;
	float x1 = x + sys->root->ws_size.x;
	float y1 = sys->root->ws_position.y;
	float y = y1 + sys->root->ws_size.y;
	impl->ViewProj = glm::orthoRH(x, x1, y, y1, -1.0f, 1.0f);
	
}
void UIBuilder::post_draw()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void UIBuilder::draw_solid_rect(glm::ivec2 global_coords,
	glm::ivec2 size,
	Color32 color)
{
	mb->Begin();
	mb->Push2dQuad(global_coords, size, glm::vec2(0, 1), glm::vec2(1, -1), color);
	mb->End();

	auto mat = (MaterialInstance*)sys->ui_default;


	auto shader = matman.get_mat_shader(false, nullptr,
		mat, false, false, false, false);

	auto& texs = mat->impl->get_textures();

	draw.set_shader(shader);

	for (int i = 0; i < texs.size(); i++)
		draw.bind_texture(i, texs.at(i)->gl_id);

	draw.shader().set_mat4("ViewProj", impl->ViewProj);

	mb->Draw(MeshBuilder::TRIANGLES);
}

static void get_uvs(glm::vec2& top_left, glm::vec2& sz, int x, int y, int w, int h, const GuiFont* f)
{
	const int tw = f->font_texture->width;
	const int th = f->font_texture->height;
	const float wf = w / (float)tw;
	const float hf = h / (float)th;
	const float xf = x / (float)tw;
	const float yf = y / (float)th;
	top_left = { xf,yf };
	sz = { wf,hf };

}

void UIBuilder::draw_text(
	glm::ivec2 global_coords,
	glm::ivec2 size,	// box for text
	const GuiFont* font,
	StringView text, Color32 color /* and alpha*/)
{
	mb->Begin();


	int x = global_coords.x;
	int y = global_coords.y - 6;//HACK fixme
	for(int i=0;i<text.str_len;i++) {
		char c = text.str_start[i];

		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10;	// empty character
		}
		else {

			glm::ivec2 coord = { x,y };
			coord.x += find->second.xofs;
			coord.y += find->second.yofs;
			glm::ivec2 sz = { find->second.w,find->second.h };

			glm::vec2 uv, uv_sz;
			get_uvs(uv, uv_sz, find->second.x, find->second.y, find->second.w, find->second.h, font);
			mb->Push2dQuad(coord, sz, uv, uv_sz, color
			);
			x += find->second.advance;
		}
	}

	mb->End();

	auto mat = (MaterialInstance*)sys->ui_default;

	auto shader = matman.get_mat_shader(false, nullptr,
		mat, false, false, false, false);

	auto& texs = mat->impl->get_textures();

	draw.set_shader(shader);

	draw.bind_texture(0, font->font_texture->gl_id);

	draw.shader().set_mat4("ViewProj", impl->ViewProj);

	mb->Draw(MeshBuilder::TRIANGLES);
}

GuiSystemPublic* GuiSystemPublic::create_gui_system() {
	return new GuiSystemLocal;
}


GUI::~GUI()
{
	if(eng->get_gui())
		eng->get_gui()->remove_reference(this);
}