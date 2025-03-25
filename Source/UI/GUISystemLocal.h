#pragma once
#include "BaseGUI.h"
#include "Framework/Hashmap.h"
#include "Framework/Hashset.h"
#include "UILoader.h"
#include <SDL2/SDL_events.h>
#include "UI/GUISystemPublic.h"
#include "Framework/MeshBuilder.h"
#include "Framework/FreeList.h"
#include "Framework/Config.h"
#include "Framework/Rect2d.h"
#include "Assets/AssetDatabase.h"
#include "Render/UIDrawPublic.h"
// Navigation, up down left right (both keyboard nad controller)

// push down "go left"
// if nothing handles it on the way back up, then try to go left on your own widget, and continue
// push down press event, same deal
// push down hover event
// when pressed, a gui can take focus and recieve key events
// also can have a list of guis that always take input

class MaterialInstance;

namespace gui {
class Fullscreen;
class BaseGUI;
}


class UIBuilder;
struct UIBuilderImpl
{
	// Ortho matrix of screen
	glm::mat4 ViewProj{};
	std::vector<UIDrawCmd> drawCmds;
	MeshBuilder meshbuilder;

	void push_scissor(Rect2d rect);
	void pop_scissor();
	void add_drawcall(MaterialInstance* mat, int start, const Texture* tex_override = nullptr);
};


extern ConfigVar ui_debug_press;


class GuiSystemLocal : public GuiSystemPublic
{
public:
	void init() final;
	GuiSystemLocal()  {
	}
	~GuiSystemLocal() {}

	void handle_event(const SDL_Event& event) final;

	void post_handle_events() final;

	void think() final;
	void paint() final;
	void sync_to_renderer() final;

	void remove_reference(gui::BaseGUI* this_panel);
	void set_focus_to_this(gui::BaseGUI* panel);

	glm::ivec2 viewport_position{0,0};
	glm::ivec2 viewport_size{0,0};

	void set_viewport_ofs(int x, int y) final {
		viewport_position = { x,y };
	}
	void set_viewport_size(int x, int y) final {
		viewport_size = { x,y };
	}
	
	UIBuilderImpl uiBuilderImpl;
	// samples from 1 texture
	// use for solid color, 1 texture ui, text
	const MaterialInstance* ui_default = nullptr;

	const MaterialInstance* get_default_ui_mat() const final {
		return ui_default;
	}
	
	gui::BaseGUI* hovering = nullptr;
	gui::BaseGUI* mouse_focus = nullptr;
	gui::BaseGUI* key_focus = nullptr;

	void set_hovering(gui::BaseGUI* panel);
	void update_widget_sizes_R(gui::BaseGUI* g);
	void update_widget_positions_R(gui::BaseGUI* g);
	void paint_widgets_R(gui::BaseGUI* g, UIBuilder& builder);
	gui::BaseGUI* find_gui_under_mouse(int x, int y, bool for_scroll) const;
	gui::BaseGUI* find_gui_under_mouse_R(gui::BaseGUI* g, int x, int y, bool for_scroll) const;


	void remove_gui_layer(gui::BaseGUI* layer);
	void add_gui_layer(gui::BaseGUI* layer);
	// -1 if not found
	int find_existing_layer(gui::BaseGUI* gui) const;
	void sort_gui_layers();

	std::vector<gui::BaseGUI*> gui_layers;
	bool layers_needs_sorting = false;
};

extern GuiSystemLocal guiSystemLocal;