#pragma once

#include "Assets/IAsset.h"
#include "Framework/MulticastDelegate.h"
#include "Game/SerializePtrHelpers.h"
#include "UI.h"
#include <glm/glm.hpp>
#include "Render/MaterialPublic.h"
#include <unordered_map>

class GuiFont;
class MeshBuilder;
class GuiSystemLocal;
struct UIBuilderImpl;
class UIBuilder
{
public:
	UIBuilder(GuiSystemLocal* sys);
	~UIBuilder();

	void init_drawing_state();
	void post_draw();

	void draw_rect_with_material(
		glm::ivec2 global_coords,
		glm::ivec2 size,
		float alpha,
		const MaterialInstance* material
	);

	void draw_rect_with_texture(
		glm::ivec2 global_coords,
		glm::ivec2 size,
		float alpha,
		const Texture* material
	);

	void draw_9box_rect(
		glm::ivec2 global_coords,
		glm::ivec2 size,
		float alpha,
		glm::vec4 margins,
		const Texture* t
	);

	void draw_solid_rect(
		glm::ivec2 global_coords,
		glm::ivec2 size,
		Color32 color
	);

	void draw_rounded_rect(
		glm::ivec2 global_coords,
		glm::ivec2 size,
		Color32 color,
		float corner_radius
	);

	void draw_text(
		glm::ivec2 global_coords,
		glm::ivec2 size,	// box for text
		const GuiFont* font,
		StringView text, Color32 color /* and alpha*/);

	void draw_text_drop_shadow(
		glm::ivec2 global_coords,
		glm::ivec2 size,	// box for text
		const GuiFont* font,
		StringView text, Color32 color, /* and alpha*/
		bool with_drop_shadow = false, Color32 drop_shadow_color = {});

private:
	MeshBuilder* mb = nullptr;
	GuiSystemLocal* sys = nullptr;
	UIBuilderImpl* impl = nullptr;
	friend class UiBuilderHelper;
};


class GuiHelpers
{
public:
	static glm::ivec2 calc_text_size(const char* str, const GuiFont* font, int force_width = -1);
	static glm::ivec2 calc_text_size_no_wrap(const char* str, const GuiFont* font);
};

enum class GuiAlignment : uint8_t
{
	Left, 
	Center, 
	Right, 
	Fill
};

struct UIAnchorPos
{
	UIAnchorPos() {
		memset(positions, 0, sizeof(positions));
	}
	uint8_t positions[2][2];

	glm::ivec2 to_screen_coord(glm::ivec2 sz, int i) const {
		assert(i >= 0 && i < 2);
		return { sz.x*(positions[0][i] / 255.f),sz.y*(positions[1][i] / 255.f) };
	}
	glm::ivec2 convert_ws_coord(int i, glm::ivec2 ls, glm::ivec2 viewport_pos, glm::ivec2 viewport_sz) const {
		auto anchor = to_screen_coord(viewport_sz, i);
		auto ofs = ls;
		return ofs + anchor + viewport_pos;
	}

	static uint8_t float_to_uint(float f) {
		int i = f * 255.f;
		return (uint8_t)glm::clamp(i, 0, 255);
	}
	static UIAnchorPos create_single(float x, float y) {
		UIAnchorPos ui;
		for (int i = 0; i < 2; i++) {
			ui.positions[0][i] = float_to_uint(x);
			ui.positions[1][i] = float_to_uint(y);
		}
		return ui;
	}

};
const static UIAnchorPos TopLeftAnchor = UIAnchorPos::create_single(0, 0);
const static UIAnchorPos TopRightAnchor = UIAnchorPos::create_single(1, 0);
const static UIAnchorPos BottomLeftAnchor = UIAnchorPos::create_single(0, 1);
const static UIAnchorPos BottomRightAnchor = UIAnchorPos::create_single(1, 1);

struct SDL_KeyboardEvent;
struct SDL_MouseWheelEvent;
CLASS_H(GUI, IAsset)
public:
	virtual ~GUI() {}

	virtual void paint(UIBuilder& builder) {}

	// updates the size of this widget
	virtual void update_widget_size() {
		desired_size = ls_sz;
	}
	// update children widget positions from the parents position, handle alignment+padding here
	virtual void update_subwidget_positions() {}

	// callbacks
	virtual void on_pressed() {}
	virtual void on_released() {}
	virtual void on_dragging() {}
	virtual void on_focus_start() {}
	virtual void on_focus_end() {}
	virtual void on_focusing() {}
	virtual void on_hover_start() {}
	virtual void on_hover_end() {}
	virtual void on_hovering() {}
	virtual void on_think() {}
	virtual void on_key_down(const SDL_KeyboardEvent& key_event) {}
	virtual void on_key_up(const SDL_KeyboardEvent& key_event) {}
	virtual void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) {}

	bool recieve_events = true;

	GUI* parent = nullptr;

	// localspace attributes for positioning
	UIAnchorPos anchor{};	// anchors this widget to a spot on the screen
	glm::ivec2 ls_position{};
	glm::ivec2 ls_sz{};
	glm::vec2 pivot_ofs{};
	GuiAlignment w_alignment{};	// how this widget is aligned in the parents size
	GuiAlignment h_alignment{};
	glm::ivec4 padding{};	// padding to apply to position

	// worldspace, these are cached on update_layout and used for input/rendering
	glm::ivec2 ws_position{};
	glm::ivec2 ws_size{};	// likely desired_size, but could be scaled
	glm::ivec2 desired_size{};


	void add_this(GUI* gui) {
		if (gui->parent == this)
			return;
		children.push_back(gui);
		//set_layout_dirty(true);
		gui->parent = this;
	}
	void remove_this(GUI* gui) {
		assert(gui->parent == this);
		for (int i = 0; i < children.size(); i++) {
			if (children[i] == gui) {
				children.erase(children.begin() + i);
				return;
			}
		}
	}
protected:
	std::vector<GUI*> children;

	friend class GuiSystemLocal;
};

