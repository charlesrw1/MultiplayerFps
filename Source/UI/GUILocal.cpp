#include "GUILocal.h"
#include "Render/DrawLocal.h"	// DrawLocal include!!
#include "glad/glad.h"
#include <glm/gtc/matrix_transform.hpp>

#include "Widgets/Layouts.h"
#include "Widgets/Visuals.h"

#include "GameEnginePublic.h"

// include

CLASS_IMPL(GuiRootPanel);
CLASS_IMPL(GUIBox);
CLASS_IMPL(GUIFullscreen);

ConfigVar ui_debug_press("ui.debug_press", "0", CVAR_BOOL | CVAR_DEV);

struct UIBuilderImpl
{
	const MaterialInstanceLocal* current_mat = nullptr;
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

	auto mat = (MaterialInstanceLocal*)sys->ui_default;


	auto shader = matman.get_mat_shader(false, nullptr,
		mat, false, false, false, false);

	auto& texs = mat->get_textures();

	draw.set_shader(shader);

	for (int i = 0; i < texs.size(); i++)
		draw.bind_texture(i, texs.at(i)->gl_id);

	draw.shader().set_mat4("ViewProj", impl->ViewProj);

	mb->Draw(MeshBuilder::TRIANGLES);
}


GuiSystemPublic* GuiSystemPublic::create_gui_system() {
	return new GuiSystemLocal;
}


GUI::~GUI()
{
	if (parent)
		parent->remove_this(this);
	if(eng->get_gui())
		eng->get_gui()->remove_reference(this);
}