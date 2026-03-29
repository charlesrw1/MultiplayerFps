#include "EditorUILayout.h"
#include "EditorDocLocal.h"

#include "UI/UILoader.h"

ConfigVar test1("test1", "200", CVAR_INTEGER, "", 0, 256);
ConfigVar test2("test2", "200", CVAR_INTEGER, "", 0, 256);
ConfigVar editor_draw_name_text("editor_draw_name_text", "0", CVAR_BOOL,
								"draw text above every entities head in editor");
ConfigVar editor_draw_name_text_alpha("editor_draw_name_text_alpha", "150", CVAR_INTEGER, "", 0, 255);

EditorUILayout::EditorUILayout(IEditorApi2& doc) : doc(doc) {

	// doc.ed_cam.on_ortho_state_change.add(this, [&]() { cube.rotation.begin_interpolate(); });
}

bool EditorUILayout::draw(EditorInputs& inputs, std::function<void()> draw_window) {
	const float dt = eng->get_dt();

	RenderWindow& window = UiSystem::inst->window;
	cube.rotation.set_current((glm::mat3)doc.camera()->get_view_setup().view);
	cube.draw(window, dt);
	// paint

	// if (doc->active_mode)
	//	doc->active_mode->draw_ui();
	draw_window();

	bool do_mouse_click =
		Input::was_mouse_released(0) && UiSystem::inst->is_vp_hovered(); // && !doc->dragger.get_is_dragging();
	int x = Input::get_mouse_pos().x;
	int y = Input::get_mouse_pos().y;

	if (!inputs.can_use_mouse_click())
		do_mouse_click = false;

	if (!eng->get_level())
		return false;
	if (!editor_draw_name_text.get_bool())
		return false;

	const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	if (!font)
		font = UiSystem::inst->defaultFont;
	auto objs = get_objs();
	std::sort(objs.begin(), objs.end(), [](const obj& a, const obj& b) -> bool { return a.pos.z < b.pos.z; });
	const Entity* clicked = nullptr;
	for (const auto o : objs) {
		string name = get_name_display_entity(o.e);

		const int icon_size = 16;
		InlineVec<Texture*, 6> icons;
		auto e = o.e;

		bool found_script = false;
		for (auto c : o.e->get_components()) {
			if (c->dont_serialize_or_edit_this())
				continue;
			const char* s = c->get_editor_outliner_icon();
			if (c->get_type().get_is_lua_class()) {
				found_script = true;
				s = "eng/editor/script_lua.png";
			}
			if (!*s)
				continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}
		if (!(found_script || editor_draw_name_text.get_bool()))
			continue;

		auto size = GuiHelpers::calc_text_size_no_wrap(name, font);

		const int text_offset = (icon_size + 1) * icons.size();
		size.w += text_offset;

		const auto coord = ndc_to_screen_coord(o.pos);
		const auto coordx = coord.x - size.w / 2;
		const auto coordy = coord.y - size.h / 2;

		const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();

		Color32 color = {50, 50, 50, (uint8_t)editor_draw_name_text_alpha.get_integer()};
		if (o.e->get_selected_in_editor())
			color = {255, 180, 0, 150};

		if (do_mouse_click) {
			Rect2d r(coordx - 3, coordy - 3, size.w + 6, size.h + 6);
			if (r.is_point_inside(x - vp_pos.x, y - vp_pos.y)) {

				clicked = o.e;
			}
		}
		glm::ivec2 textofs = {0, font->base};

		RectangleShape shape;
		shape.rect = Rect2d({coordx - 3, coordy - 3}, {size.w + 6, size.h + 6});
		shape.color = color;
		window.draw(shape);

		// builder.draw_solid_rect({ coordx - 3,coordy - 3 }, { size.w + 6,size.h + 6 }, color);
		for (int i = 0; i < icons.size(); i++) {
			const int ofs = (i) * (icon_size + 1);

			shape.rect = Rect2d({coordx + ofs, coordy}, {icon_size, icon_size});
			shape.texture = icons[i];
			shape.color = COLOR_WHITE;
			window.draw(shape);
		}

		TextShape tshape;
		tshape.rect = Rect2d(glm::ivec2{coordx + 1 + text_offset, coordy + 1} + textofs, {});
		tshape.font = font;
		tshape.text = name;
		tshape.color = COLOR_BLACK;
		window.draw(shape);
		tshape.rect = Rect2d(glm::ivec2{coordx + text_offset, coordy} + textofs, {});
		tshape.color = COLOR_WHITE;
		window.draw(tshape);
	}
	if (clicked) {
		inputs.eat_mouse_click();
		auto* selection = doc.selection();
		if (Input::is_shift_down()) {
			selection->do_selection(MouseSelectionAction::ADD_SELECT, clicked);
		} else if (Input::is_ctrl_down()) {
			selection->do_selection(MouseSelectionAction::UNSELECT, clicked);
		} else {
			selection->do_selection(MouseSelectionAction::SELECT_ONLY, clicked);
		}
		return true;
	} else {
		// if(do_mouse_click)
		//	mouse_down_delegate.invoke(x-ws_position.x, y-ws_position.y, button_clicked);
	}
	return false;
}

void EditorUILayout::do_box_select(MouseSelectionAction action, Rect2d area) {
	if (!editor_draw_name_text.get_bool())
		return;
	if (!eng->get_level())
		return;

	auto objs = get_objs();

	const auto vp_size = UiSystem::inst->get_vp_rect().get_size();
	const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();

	area.x -= vp_pos.x;
	area.y -= vp_pos.y;

	auto* selection = doc.selection();
	for (auto o : objs) {
		const char* name = (o.e->get_editor_name().c_str());
		const bool is_prefab_root =
			false; // o.e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
		if (!*name) {
			if (is_prefab_root) {
				// name = o.e->get_object_prefab().get_name().c_str();
			} else {
				if (auto m = o.e->get_component<MeshComponent>()) {
					if (m->get_model())
						name = m->get_model()->get_name().c_str();
				}
			}
		}
		if (!*name) {
			name = o.e->get_type().classname;
		}
		const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
		if (!font)
			font = UiSystem::inst->defaultFont;
		const int icon_size = 16;
		InlineVec<Texture*, 6> icons;
		auto e = o.e;
		if (is_prefab_root) {
			const char* s = "eng/editor/prefab_p.png";
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		for (auto c : o.e->get_components()) {
			if (c->dont_serialize_or_edit_this())
				continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s)
				continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		auto size = GuiHelpers::calc_text_size_no_wrap(name, font);

		const int text_offset = (icon_size + 1) * icons.size();
		size.w += text_offset;

		o.pos.y *= -1;
		auto coordx = o.pos.x * 0.5 + 0.5;
		auto coordy = o.pos.y * 0.5 + 0.5;

		coordx *= vp_size.x;
		coordy *= vp_size.y;
		// coordx += vp_pos.x;
		// coordy += vp_pos.y;
		coordx -= size.w / 2;
		coordy -= size.h / 2;

		Rect2d r(coordx - 3, coordy - 3, size.w + 6, size.h + 6);
		if (r.overlaps(area)) {
			selection->do_selection(action, e);
		}
	}
}

std::vector<EditorUILayout::obj> EditorUILayout::get_objs() {
	std::vector<obj> objs;
	auto& all_objs = eng->get_level()->get_all_objects();
	View_Setup setup = doc.camera()->get_view_setup();
	for (auto o : all_objs) {
		if (Entity* e = o->cast_to<Entity>()) {
			if (!this_is_a_serializeable_object(e))
				continue;
			obj ob;
			glm::vec3 todir = glm::vec3(e->get_ws_position()) - setup.origin;
			float dist = glm::dot(todir, todir);
			if (dist > 20.0 * 20.0)
				continue;
			ob.e = e;
			glm::vec4 pos = setup.viewproj * glm::vec4(e->get_ws_position(), 1.0);
			ob.pos = pos / pos.w;

			if (ob.pos.z < 0)
				continue;

			objs.push_back(ob);
		}
	}
	return objs;
}
