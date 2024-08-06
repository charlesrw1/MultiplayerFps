#pragma once
#include "GUIPublic.h"
#include "Framework/Hashmap.h"
#include "Framework/Hashset.h"
#include "UILoader.h"
#include <SDL2/SDL_events.h>
#include "UI/GUISystemPublic.h"
#include "Framework/MeshBuilder.h"
#include "Framework/FreeList.h"
#include "Framework/Config.h"
#include "Framework/Rect2d.h"
// Navigation, up down left right (both keyboard nad controller)

// push down "go left"
// if nothing handles it on the way back up, then try to go left on your own widget, and continue
// push down press event, same deal
// push down hover event
// when pressed, a gui can take focus and recieve key events
// also can have a list of guis that always take input


CLASS_H(GuiRootPanel, GUI)
public:
	void update_subwidget_positions() override {
		for (int i = 0; i < children.size(); i++) {
			children[i]->ws_position = ws_position;
			children[i]->ws_size = ws_size;
		}
	}
};

extern ConfigVar ui_debug_press;

class GuiSystemLocal : public GuiSystemPublic
{
public:
	GuiSystemLocal() {//: think_list(3) {
		root = new GuiRootPanel;


		ui_default = imaterials->find_material_instance("uiDefault");
		if (!ui_default)
			Fatalf("Couldnt find default ui material");
	}
	~GuiSystemLocal() {}

	void handle_event(const SDL_Event& event) override {
		switch (event.type) {
		case SDL_KEYDOWN: {
			if (focusing)
				focusing->on_key_down(event.key);
		}break;
		case SDL_KEYUP: {
			if (focusing)
				focusing->on_key_up(event.key);
		}break;
		case SDL_MOUSEWHEEL: {
			if (focusing)
				focusing->on_mouse_scroll(event.wheel);
		}break;

		case SDL_MOUSEBUTTONDOWN: {
			GUI* g = find_gui_under_mouse_R(root, event.button.x, event.button.y);
			if (g) {
				const glm::ivec2 where = { event.button.x - g->ws_position.x,event.button.y - g->ws_position.y };
				
				if (ui_debug_press.get_bool())
					sys_print("*** UI pressed: %s\n", g->get_type().classname);
				g->on_pressed(where.x, where.y, event.button.button);

				if (event.button.button == 1)
				{
					dragging = g;
				}
			}
		}break;
		case SDL_MOUSEBUTTONUP: {
			if (event.button.button == 1) {
				if (dragging) {
					if (ui_debug_press.get_bool())
						sys_print("*** UI dragged released: %s\n", dragging->get_type().classname);

					const glm::ivec2 where = { event.button.x - dragging->ws_position.x,event.button.y - dragging->ws_position.y };
					dragging->on_released(where.x, where.y, 1);
					dragging = nullptr;
				}
			}
			else {
				GUI* g = find_gui_under_mouse_R(root, event.button.x, event.button.y);
				if (g) {
					if (ui_debug_press.get_bool())
						sys_print("*** UI released: %s\n", g->get_type().classname);

					const glm::ivec2 where = { event.button.x - g->ws_position.x,event.button.y - g->ws_position.y };
					g->on_released(where.x, where.y, event.button.button);
				}
			}
		}break;
		}
	}

	void post_handle_events() override {
		if (!dragging) {
			int x = 0, y = 0;
			SDL_GetMouseState(&x, &y);
			GUI* g = find_gui_under_mouse_R(root, (int16_t)x, (int16_t)y);
			set_hovering(g);
		}
	}

	void think() override {
		CPUFUNCTIONSTART;

		int x = 0, y = 0;
		SDL_GetMouseState(&x, &y);

		if (hovering)
			hovering->on_hovering(x,y);
		if (dragging)
			dragging->on_dragging(x,y);
		if (focusing)
			focusing->on_focusing();
		//for (auto gui : think_list)
		//	gui->on_think();

		update_widget_sizes_R(root);
		update_widget_positions_R(root);
	}
	void paint() override {
		GPUFUNCTIONSTART;

		UIBuilder build(this);
		build.init_drawing_state();
		paint_widgets_R(root, build);
		build.post_draw();
	}

	void add_gui_panel_to_root(GUI* panel) override {
		root->add_this(panel);
	}
	void remove_reference(GUI* this_panel) override {
		if (hovering == this_panel) hovering = nullptr;
		if (dragging == this_panel) dragging = nullptr;
		if (focusing == this_panel) focusing = nullptr;
		//think_list.remove(this_panel);
	}
	void set_focus_to_this(GUI* panel) override {
		if (focusing == panel)
			return;
		if (focusing) {
			if (ui_debug_press.get_bool())
				sys_print("*** UI focus end: %s\n", focusing->get_type().classname);
			focusing->on_focus_end();
		}
		focusing = panel;
		if (panel) {
			if (ui_debug_press.get_bool())
				sys_print("*** UI focus start: %s\n", focusing->get_type().classname);
			panel->on_focus_start();
		}
	}

	void set_viewport_ofs(int x, int y) override {
		root->ws_position = { x,y };
	}
	void set_viewport_size(int x, int y) override {
		root->ws_size = { x,y };
	}
	void add_to_think_list(GUI* panel) override {
		//think_list.insert(panel);
	}
	void remove_from_think_list(GUI* panel) override {
		//think_list.remove(panel);
	}

	handle<World_GUI> register_world_gui(const World_GUI& wgui) override {
		return { -1 };
	}
	void update_world_gui(handle<World_GUI> handle, const World_GUI& wgui) override {
	
	}
	void remove_world_gui(handle<World_GUI>& handle) override {

	}



	// local interface

	// samples from 1 texture
	// use for solid color, 1 texture ui, text
	const MaterialInstance* ui_default = nullptr;


	GUI* hovering = nullptr;
	GUI* dragging = nullptr;
	GUI* focusing = nullptr;

	//hash_set<GUI> think_list;

	void set_hovering(GUI* panel) {
		if (hovering == panel)
			return;
		if (hovering) {
			if (ui_debug_press.get_bool())
				sys_print("*** UI hover end: %s\n", hovering->get_type().classname);
			hovering->on_hover_end();
		}
		hovering = panel;
		if (panel) {
			if (ui_debug_press.get_bool())
				sys_print("*** UI hover start: %s\n", hovering->get_type().classname);
			panel->on_hover_start();
		}
	}

	void update_widget_sizes_R(GUI* g) {
		for (int i = 0; i < g->children.size(); i++)
			update_widget_sizes_R(g->children[i].get());
		g->update_widget_size();
	}
	void update_widget_positions_R(GUI* g) {
		g->update_subwidget_positions();
		for (int i = 0; i < g->children.size(); i++)
			update_widget_positions_R(g->children[i].get());
	}
	void paint_widgets_R(GUI* g, UIBuilder& builder) {
		g->paint(builder);
		for (int i = 0; i < g->children.size(); i++)
			paint_widgets_R(g->children[i].get(), builder);
	}

	GUI* find_gui_under_mouse_R(GUI* g, int16_t x, int16_t y) {
		Rect2d r(g->ws_position.x,g->ws_position.y,g->ws_size.x,g->ws_size.y);
		if (!r.is_point_inside(x, y))
			return nullptr;
		for (int i = 0; i < g->children.size(); i++) {
			auto child = g->children[i].get();
			GUI* g_sub = find_gui_under_mouse_R(child, x, y);
			if (g_sub&&g_sub->recieve_events)
				return g_sub;
		}
		return g;
	}


	GuiRootPanel* root = nullptr;
};
