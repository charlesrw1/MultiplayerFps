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
#include "Assets/AssetDatabase.h"

#include "Render/UIDrawPublic.h"

// Navigation, up down left right (both keyboard nad controller)

// push down "go left"
// if nothing handles it on the way back up, then try to go left on your own widget, and continue
// push down press event, same deal
// push down hover event
// when pressed, a gui can take focus and recieve key events
// also can have a list of guis that always take input


CLASS_H(GuiRootPanel, GUI)
public:
	void update_subwidget_positions() final {
		for (int i = 0; i < children.size(); i++) {
			children[i]->ws_position = ws_position;
			children[i]->ws_size = ws_size;
		}
	}
};


struct UIBuilderImpl
{
	// Ortho matrix of screen
	glm::mat4 ViewProj{};
	std::vector<UIDrawCall> drawCalls;
	MeshBuilder meshbuilder;

	void add_drawcall(MaterialInstance* mat, int start) {
		const int count = meshbuilder.get_i().size() - start;
		if (drawCalls.empty() || drawCalls.back().mat != mat) {
			UIDrawCall dc;
			dc.index_count = count;
			dc.index_start = start;
			dc.mat = mat;
			drawCalls.push_back(dc);
		}
		drawCalls.back().index_count += count;
	}
};


extern ConfigVar ui_debug_press;

class GuiSystemLocal : public GuiSystemPublic
{
public:
	GuiSystemLocal() {//: think_list(3) {
		root = new GuiRootPanel;


		ui_default = g_assets.find_global_sync<MaterialInstance>("eng/uiDefault.mm").get();
		if (!ui_default)
			Fatalf("Couldnt find default ui material");
	}
	~GuiSystemLocal() {}

	void handle_event(const SDL_Event& event) final {
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
					sys_print(Debug, "UI pressed: %s\n", g->get_type().classname);
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
						sys_print(Debug, "UI dragged released: %s\n", dragging->get_type().classname);

					const glm::ivec2 where = { event.button.x - dragging->ws_position.x,event.button.y - dragging->ws_position.y };
					dragging->on_released(where.x, where.y, 1);
					dragging = nullptr;
				}
			}
			else {
				GUI* g = find_gui_under_mouse_R(root, event.button.x, event.button.y);
				if (g) {
					if (ui_debug_press.get_bool())
						sys_print(Debug, "UI released: %s\n", g->get_type().classname);

					const glm::ivec2 where = { event.button.x - g->ws_position.x,event.button.y - g->ws_position.y };
					g->on_released(where.x, where.y, event.button.button);
				}
			}
		}break;
		}
	}

	void post_handle_events() final {
		if (!dragging) {
			int x = 0, y = 0;
			SDL_GetMouseState(&x, &y);
			GUI* g = find_gui_under_mouse_R(root, (int16_t)x, (int16_t)y);
			set_hovering(g);
		}
	}

	void think() final {
	
		int x = 0, y = 0;
		SDL_GetMouseState(&x, &y);

		if (hovering && hovering->hidden)
			hovering = nullptr;
		if (dragging && dragging->hidden)
			dragging = nullptr;
		if (focusing && focusing->hidden)
			focusing = nullptr;

		if (hovering) {
			const glm::ivec2 where = { x - hovering->ws_position.x,y - hovering->ws_position.y };
			hovering->on_hovering(where.x, where.y);
		}
		if (dragging) {
			const glm::ivec2 where = { x - dragging->ws_position.x,y - dragging->ws_position.y };
			dragging->on_dragging(where.x, where.y);
		
		}
		if (focusing)
			focusing->on_focusing();
		//for (auto gui : think_list)
		//	gui->on_think();

		update_widget_sizes_R(root);
		update_widget_positions_R(root);
	}
	UIBuilderImpl uiBuilderImpl;
	void paint() final {
		UIBuilder build(this);
		paint_widgets_R(root, build);
	}
	void sync_to_renderer() final {
		idrawUi->update(uiBuilderImpl.drawCalls, uiBuilderImpl.meshbuilder, uiBuilderImpl.ViewProj);
	}

	void add_gui_panel_to_root(GUI* panel) final {
		root->add_this(panel);
	}
	void remove_reference(GUI* this_panel) final {
		if (hovering == this_panel) hovering = nullptr;
		if (dragging == this_panel) dragging = nullptr;
		if (focusing == this_panel) focusing = nullptr;
		//think_list.remove(this_panel);
	}
	void set_focus_to_this(GUI* panel) final {
		if (focusing == panel)
			return;
		if (focusing) {
			if (ui_debug_press.get_bool())
				sys_print(Debug, "UI focus end: %s\n", focusing->get_type().classname);
			focusing->on_focus_end();
		}
		focusing = panel;
		if (panel) {
			if (ui_debug_press.get_bool())
				sys_print(Debug, "UI focus start: %s\n", focusing->get_type().classname);
			panel->on_focus_start();
		}
	}

	void set_viewport_ofs(int x, int y) final {
		root->ws_position = { x,y };
	}
	void set_viewport_size(int x, int y) final {
		root->ws_size = { x,y };
	}
	void add_to_think_list(GUI* panel) final {
		//think_list.insert(panel);
	}
	void remove_from_think_list(GUI* panel) final {
		//think_list.remove(panel);
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
				sys_print(Debug, "UI hover end: %s\n", hovering->get_type().classname);
			hovering->on_hover_end();
		}
		hovering = panel;
		if (panel) {
			if (ui_debug_press.get_bool())
				sys_print(Debug, "UI hover start: %s\n", hovering->get_type().classname);
			panel->on_hover_start();
		}
	}

	void update_widget_sizes_R(GUI* g) {
		if (g->hidden)
			return;
		for (int i = 0; i < g->children.size(); i++)
			update_widget_sizes_R(g->children[i].get());
		g->update_widget_size();
	}
	void update_widget_positions_R(GUI* g) {
		if (g->hidden)
			return;
		g->update_subwidget_positions();
		for (int i = 0; i < g->children.size(); i++)
			update_widget_positions_R(g->children[i].get());
	}
	void paint_widgets_R(GUI* g, UIBuilder& builder) {
		if (g->hidden)
			return;
		g->paint(builder);
		for (int i = 0; i < g->children.size(); i++)
			paint_widgets_R(g->children[i].get(), builder);
	}

	GUI* find_gui_under_mouse_R(GUI* g, int16_t x, int16_t y) {
		if (g->hidden)
			return nullptr;

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
