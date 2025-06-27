#pragma once
#include <string_view>
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include <glm/glm.hpp>

class MaterialInstance;
class GuiFont;
class MeshBuilder;
class GuiSystemLocal;
struct UIBuilderImpl;
class Texture;
class UIBuilder
{
public:
	UIBuilder(GuiSystemLocal* sys);
	~UIBuilder();

	void draw_rect_with_material(glm::ivec2 global_coords, glm::ivec2 size, float alpha, const MaterialInstance* material);
	void draw_rect_with_texture(glm::ivec2 global_coords, glm::ivec2 size, float alpha,const Texture* material);
	void draw_9box_rect(glm::ivec2 global_coords,glm::ivec2 size,float alpha,glm::vec4 margins,const Texture* t);
	void draw_solid_rect(glm::ivec2 global_coords,glm::ivec2 size,Color32 color);
	void draw_rounded_rect(glm::ivec2 global_coords,glm::ivec2 size,Color32 color,float corner_radius);
	void draw_text(glm::ivec2 global_coords,glm::ivec2 size,const GuiFont* font,std::string_view text, Color32 color /* and alpha*/);
	void draw_text_drop_shadow(glm::ivec2 global_coords,glm::ivec2 size,const GuiFont* font,std::string_view text, 
		Color32 color, /* and alpha*/ bool with_drop_shadow = false, Color32 drop_shadow_color = {});
	// manual drawing
	MeshBuilder& get_meshbuilder();
	void add_drawcall(int start_index, const MaterialInstance* material, const Texture* override);
private:
	GuiSystemLocal* sys = nullptr;
	UIBuilderImpl* impl = nullptr;
	friend class UiBuilderHelper;
};

class GuiHelpers
{
public:
	static Rect2d calc_text_size(const char* str, const GuiFont* font, int force_width = -1);
	static Rect2d calc_text_size_no_wrap(const char* str, const GuiFont* font);
};

