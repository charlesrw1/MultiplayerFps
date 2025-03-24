#include "GUISystemLocal.h"
#include "Widgets/Layouts.h"
#include "UIBuilder.h"
#include <algorithm>
#include "Render/MaterialPublic.h"

GuiSystemLocal guiSystemLocal;
GuiSystemPublic* g_guiSystem = &guiSystemLocal;

void GuiSystemLocal::init() {
	ui_default = g_assets.find_global_sync<MaterialInstance>("eng/uiDefault.mm").get();
	if (!ui_default)
		Fatalf("Couldnt find default ui material");
}

void GuiSystemLocal::handle_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_KEYDOWN: {
		if (key_focus)
			key_focus->on_key_down(event.key);
	}break;
	case SDL_KEYUP: {
		if (key_focus)
			key_focus->on_key_up(event.key);
	}break;
	case SDL_MOUSEWHEEL: {
		if (key_focus)
			key_focus->on_mouse_scroll(event.wheel);
	}break;

	case SDL_MOUSEBUTTONDOWN: {
		gui::BaseGUI* g = find_gui_under_mouse(event.button.x, event.button.y);
		if (g) {
			const glm::ivec2 where = { event.button.x - g->ws_position.x,event.button.y - g->ws_position.y };

			if (ui_debug_press.get_bool())
				sys_print(Debug, "UI pressed: %s\n", g->get_type().classname);
			g->on_pressed(where.x, where.y, event.button.button);

			if (event.button.button == 1) {
				mouse_focus = g;
			}
		}
	}break;
	case SDL_MOUSEBUTTONUP: {
		if (event.button.button == 1) {
			if (mouse_focus) {
				if (ui_debug_press.get_bool())
					sys_print(Debug, "UI dragged released: %s\n", mouse_focus->get_type().classname);

				const glm::ivec2 where = { event.button.x - mouse_focus->ws_position.x,event.button.y - mouse_focus->ws_position.y };
				mouse_focus->on_released(where.x, where.y, 1);
				mouse_focus = nullptr;
			}
		}
	}break;
	}
}

void GuiSystemLocal::post_handle_events() {
	sort_gui_layers();
	{
		int x = 0, y = 0;
		SDL_GetMouseState(&x, &y);
		gui::BaseGUI* g = find_gui_under_mouse(x, y);
		set_hovering(g);
	}
}

void GuiSystemLocal::think() {

	sort_gui_layers();

	int x = 0, y = 0;
	SDL_GetMouseState(&x, &y);

	if (hovering && hovering->get_is_hidden())
		hovering = nullptr;
	if (mouse_focus && mouse_focus->get_is_hidden())
		mouse_focus = nullptr;
	if (key_focus && key_focus->get_is_hidden())
		key_focus = nullptr;

	if (hovering) {
		const glm::ivec2 where = { x - hovering->ws_position.x,y - hovering->ws_position.y };
		hovering->on_hovering(where.x, where.y);
	}
	if (mouse_focus) {
		const glm::ivec2 where = { x - mouse_focus->ws_position.x,y - mouse_focus->ws_position.y };
		mouse_focus->on_dragging(where.x, where.y);
	}
	if (key_focus)
		key_focus->on_focusing();

	//for (auto gui : think_list)
	//	gui->on_think();

	for (auto l : gui_layers) {
		if (auto f = l->cast_to<gui::Fullscreen>()) {
			f->set_ls_position(viewport_position);
			f->ws_position = viewport_position;
			f->set_ls_size(viewport_size);
			f->ws_size = viewport_size;
		}
		update_widget_sizes_R(l);
	}
	for (auto l : gui_layers)
		update_widget_positions_R(l);
}

void GuiSystemLocal::paint() {
	UIBuilder build(this);
	for (int i = gui_layers.size() - 1;i>=0; i--) {
		paint_widgets_R(gui_layers[i], build);
	}
}

void GuiSystemLocal::sync_to_renderer() {
	idrawUi->update(uiBuilderImpl.drawCalls, uiBuilderImpl.meshbuilder, uiBuilderImpl.ViewProj);
}

void GuiSystemLocal::remove_reference(gui::BaseGUI* this_panel) {
	if (hovering == this_panel) {
		hovering->on_hover_end();
		hovering = nullptr;
	}
	if (mouse_focus == this_panel) {
		mouse_focus = nullptr;
	}
	if (key_focus == this_panel) {
		key_focus->on_focus_end();
		key_focus = nullptr;
	}

	if (this_panel->is_a_gui_root) {
		remove_gui_layer(this_panel);
	}
	ASSERT(find_existing_layer(this_panel) == -1);

}

void GuiSystemLocal::set_focus_to_this(gui::BaseGUI* panel) {
	if (key_focus == panel)
		return;
	if (key_focus) {
		if (ui_debug_press.get_bool())
			sys_print(Debug, "UI focus end: %s\n", key_focus->get_type().classname);
		key_focus->on_focus_end();
	}
	key_focus = panel;
	if (key_focus) {
		if (ui_debug_press.get_bool())
			sys_print(Debug, "UI focus start: %s\n", key_focus->get_type().classname);
		key_focus->on_focus_start();
	}
}

void GuiSystemLocal::set_hovering(gui::BaseGUI* panel) {
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

void GuiSystemLocal::update_widget_sizes_R(gui::BaseGUI* g) {
	if (g->get_is_hidden())
		return;
	InlineVec<gui::BaseGUI*, 16> children;
	g->get_gui_children(children);
	for (int i = 0; i < children.size(); i++)
		update_widget_sizes_R(children[i]);
	g->update_widget_size();
}

void GuiSystemLocal::update_widget_positions_R(gui::BaseGUI* g) {
	if (g->get_is_hidden())
		return;
	g->update_subwidget_positions();
	InlineVec<gui::BaseGUI*, 16> children;
	g->get_gui_children(children);
	for (int i = 0; i < children.size(); i++)
		update_widget_positions_R(children[i]);
}

void GuiSystemLocal::paint_widgets_R(gui::BaseGUI* g, UIBuilder& builder) {
	if (g->get_is_hidden())
		return;
	g->paint(builder);
	InlineVec<gui::BaseGUI*, 16> children;
	g->get_gui_children(children);
	for (int i = 0; i < children.size(); i++)
		paint_widgets_R(children[i], builder);
}

gui::BaseGUI* GuiSystemLocal::find_gui_under_mouse(int x, int y) const {
	for (auto l : gui_layers) {
		auto f = find_gui_under_mouse_R(l, x, y);
		if (f)
			return f;
	}
	return nullptr;
}

gui::BaseGUI* GuiSystemLocal::find_gui_under_mouse_R(gui::BaseGUI* g, int x, int y) const {
	if (g->get_is_hidden() || !g->recieve_events)
		return nullptr;

	Rect2d r(g->ws_position.x, g->ws_position.y, g->ws_size.x, g->ws_size.y);
	if (!r.is_point_inside(x, y))
		return nullptr;
	InlineVec<gui::BaseGUI*, 16> children;
	g->get_gui_children(children);
	for (int i = 0; i < children.size(); i++) {
		auto child = children[i];
		gui::BaseGUI* g_sub = find_gui_under_mouse_R(child, x, y);
		if (g_sub && g_sub->recieve_events)
			return g_sub;
	}
	return g;
}

void UIBuilderImpl::add_drawcall(MaterialInstance* mat, int start, const Texture* tex_override) {
	const int count = meshbuilder.get_i().size() - start;
	if (drawCalls.empty() || drawCalls.back().mat != mat || drawCalls.back().texOverride != tex_override) {
		UIDrawCall dc;
		dc.index_count = count;
		dc.index_start = start;
		dc.mat = mat;
		dc.texOverride = tex_override;
		drawCalls.push_back(dc);
	}
	else
		drawCalls.back().index_count += count;
}

void GuiSystemLocal::sort_gui_layers()
{
	if(!layers_needs_sorting)
		return;
	layers_needs_sorting = false;

	std::sort(gui_layers.begin(), gui_layers.end(), [](const gui::BaseGUI* a, const gui::BaseGUI* b)->bool {
		auto af = a->cast_to<gui::Fullscreen>();
		auto bf = b->cast_to<gui::Fullscreen>();
		int ai = af ?  af->z_order : 10000000;
		int bi = bf ? bf->z_order : 10000000;
		return ai > bi;
		});
}
void GuiSystemLocal::remove_gui_layer(gui::BaseGUI* layer) {
	int i = find_existing_layer(layer);
	if (i == -1) {
		sys_print(Warning, "couldnt remove gui layer, not found\n");
	}
	else {
		sys_print(Info, "removing layer\n");
		gui_layers.erase(gui_layers.begin() + i);

	}
	layer->is_a_gui_root = false;

}
void GuiSystemLocal::add_gui_layer(gui::BaseGUI* layer) {
	sys_print(Info, "adding layer\n");

	ASSERT(find_existing_layer(layer) == -1);
	gui_layers.push_back(layer);
	layers_needs_sorting = true;
	layer->is_a_gui_root = true;
}

int GuiSystemLocal::find_existing_layer(gui::BaseGUI* gui) const {
	for (int i = 0; i < gui_layers.size(); i++)
		if (gui_layers[i] == gui)
			return i;
	return -1;
}