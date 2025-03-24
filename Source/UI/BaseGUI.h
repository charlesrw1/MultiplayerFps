#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Framework/ClassBase.h"
#include "Framework/StringUtil.h"	// string view
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include "Game/EntityComponent.h"
#include "Framework/InlineVec.h"

template<typename ...Args>
class MulticastDelegate;


struct SDL_KeyboardEvent;
struct SDL_MouseWheelEvent;
class GuiSystemLocal;
class UIBuilder;

NEWENUM(guiAlignment,uint8_t)
{
	Left, 
	Center, 
	Right, 
	Fill
};

namespace gui
{


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

NEWCLASS(BaseGUI, EntityComponent)
public:
	BaseGUI();
	virtual ~BaseGUI();

#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guibox.png"; }
#endif

	void start() override;
	void on_changed_transform() final; //from EntityComponent

	virtual void paint(UIBuilder& builder) {}

	// updates the size of this widget
	virtual void update_widget_size() {
		desired_size = get_ls_size();
	}
	// update children widget positions from the parents position, handle alignment+padding here
	virtual void update_subwidget_positions() {}

	// callbacks, return true if uses event
	virtual void on_pressed(int x, int y, int button) { }
	virtual void on_released(int x, int y, int button) { }
	virtual void on_dragging(int x, int y) {}

	virtual void on_focus_start() {}
	virtual void on_focus_end() {}
	virtual void on_focusing() {}
	virtual void on_key_down(const SDL_KeyboardEvent& key_event) { }
	virtual void on_key_up(const SDL_KeyboardEvent& key_event) { }
	virtual void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) {}

	virtual void on_custom_event(const char* str, bool pressed) {}

	virtual void on_hover_start() {}
	virtual void on_hover_end() {}
	virtual void on_hovering(int x, int y) {}

	virtual void on_think() {}

	bool has_focus() const;
	void set_focus();
	void release_focus();
	bool is_dragging() const;
	bool is_hovering() const;

	// helpers
	void get_gui_children(InlineVec<BaseGUI*, 16>& outvec) const;
	BaseGUI* get_gui_parent() const;
	bool get_is_hidden() const;

	bool recieve_events = true;
	bool hidden = false;
	bool is_a_gui_root = false;

	// localspace attributes for positioning
	UIAnchorPos anchor {};	// anchors this widget to a spot on the screen

	REFLECT();
	int ls_x = 0;
	REFLECT();
	int ls_y = 0;
	REFLECT();
	int ls_w = 50;
	REFLECT();
	int ls_h = 50;

	glm::ivec2 get_ls_position() const {
		return { ls_x,ls_y };
	}
	glm::ivec2 get_ls_size() const {
		return { ls_w,ls_h };
	}
	void set_ls_size(glm::ivec2 sz) {
		ls_w = sz.x;
		ls_h = sz.y;
	}
	void set_ls_position(glm::ivec2 p) {
		ls_x = p.x;
		ls_y = p.y;
	}

	REFLECT();
	float pivot_x = 0.f;
	REFLECT();
	float pivot_y = 0.f;

	glm::vec2 get_pivot_ofs() const {
		return { pivot_x,pivot_y };
	}
	
	REFLECT();
	guiAlignment w_alignment = guiAlignment::Left;	// how this widget is aligned in the parents size
	REFLECT();
	guiAlignment h_alignment = guiAlignment::Left;
	
	REFLECT();
	int16_t padding_r = 0;
	REFLECT();
	int16_t padding_l = 0;
	REFLECT();
	int16_t padding_u = 0;
	REFLECT();
	int16_t padding_d = 0;

	glm::ivec4 get_padding() const {
		return { padding_r,padding_l,padding_u,padding_d };
	}

	REFLECT();
	bool use_desired_size = false;

	// worldspace, these are cached on update_layout and used for input/rendering
	glm::ivec2 ws_position{};
	glm::ivec2 ws_size{};	// likely desired_size, but could be scaled
	glm::ivec2 desired_size{};

protected:
	
	friend class ::GuiSystemLocal;
private:
};
}

