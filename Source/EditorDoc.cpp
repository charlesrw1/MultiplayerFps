#include "EditorDocLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "glm/gtx/euler_angles.hpp"
#include "External/ImGuizmo.h"

#include "Framework/MeshBuilder.h"
#include "Framework/Dict.h"
#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "Framework/DictWriter.h"

#include "Physics/Physics2.h"

#include "EditorFolder.h"
#include "OsInput.h"
#include "Debug.h"

#include "Game/StdEntityTypes.h"
#include "Game/Schema.h"

#include <algorithm>
#include <stdexcept>
#include <fstream>

#include "Render/DrawPublic.h"
#include "Render/Texture.h"

#include "AssetCompile/Someutils.h"
#include "AssetCompile/Someutils.h"// string stuff
#include "Assets/AssetRegistry.h"

#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Interactables.h"
#include "UI/Widgets/Visuals.h"
#include "UI/GUISystemPublic.h"

EditorDoc ed_doc;
IEditorTool* g_editor_doc = &ed_doc;


class EditorUILayout : public GUIFullscreen
{
public:

	static GUIButton* create_button(const char* textstr) {
		GUIButton* button = new GUIButton;
		button->padding = { 5,5,5,5 };
		button->w_alignment = GuiAlignment::Fill;

		GUIText* text = new GUIText;
		text->text = textstr;
		text->color = COLOR_WHITE;
		text->ls_position = { 0,0 };
		text->w_alignment = GuiAlignment::Left;
		text->h_alignment = GuiAlignment::Center;
		text->padding = { 10,10,10,10 };


		button->add_this(text);

		return button;
	}

	EditorUILayout() {
		test = new GUIBox;
		test->color = COLOR_RED;
		test->ls_sz = { 100,100 };
		test->ls_position = { 0,0 };
		test->pivot_ofs = { 0.5,0.5 };
		test->padding = { 5,5,5,5 };
		test->h_alignment = GuiAlignment::Center;
		test->w_alignment = GuiAlignment::Left;

		vbox = new GUIVerticalBox;
		vbox->ls_position = { 100,100 };
		vbox->ls_sz = { 200,500 };

	
		GUIButton* b = create_button("PLAY");
		vbox->add_this(b);
		b = create_button("EXIT GAME");
		vbox->add_this(b);

		add_this(vbox);
		//t2 = new GUIText;
		//t2->text = "something else that I want to say";
		//t2->ls_position = { 0,0 };
		//t2->use_desired_size = true;
		//add_this(t2);
	}
	void print_something() {
		sys_print("---------------HELLO WORLD---------------\n");
	}

	void on_pressed(int x, int y, int button) override {
		eng->get_gui()->set_focus_to_this(this);
		mouse_down_delegate.invoke(x, y, button);
	}
	void on_released(int x, int y, int button) override {
		mouse_up_delegate.invoke(x, y, button);
	}
	void on_key_down(const SDL_KeyboardEvent& key_event) override {
		key_down_delegate.invoke(key_event);
	}
	void on_key_up(const SDL_KeyboardEvent& key_event) override {
		key_up_delegate.invoke(key_event);
	}
	void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) override {
		wheel_delegate.invoke(wheel);
	}

	MulticastDelegate<const SDL_KeyboardEvent&> key_down_delegate;
	MulticastDelegate<const SDL_KeyboardEvent&> key_up_delegate;
	MulticastDelegate<int, int, int> mouse_down_delegate;
	MulticastDelegate<int, int, int> mouse_up_delegate;
	MulticastDelegate<const SDL_MouseWheelEvent&> wheel_delegate;



	GUIBox* test = nullptr;

	GUIVerticalBox* vbox = nullptr;
};

CLASS_IMPL(EditorFolder);


static std::string to_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}

// Create from a class, create from a schema, create from a duplication
// -> serialize to use as an interchange format

class CreateSchemaCommand : public Command
{
public:
	CreateSchemaCommand(const std::string& schemaname, const glm::mat4& transform) {
		s = g_schema_loader.load_schema(schemaname);
		this->transform = transform;
	}

	bool is_valid() override { return s != nullptr; }

	void execute() {
		auto ent = eng->spawn_entity_schema(s);
		ent->set_ws_transform(transform);
		ent->editor_name = s->get_name();

		handle = ent->self_id.handle;
		ed_doc.on_node_created.invoke(handle);
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		ed_doc.on_node_will_delete.invoke(handle);
		eng->remove_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = 0;
	}
	std::string to_string() override {
		return "CreateSchemaCommand";
	}

	uint64_t handle = 0;
	const Schema* s=nullptr;
	glm::mat4 transform;
};
class CreateStaticMeshCommand : public Command
{
public:
	CreateStaticMeshCommand(const std::string& modelname, const glm::mat4& transform) {
		m = mods.find_or_load(modelname.c_str());
		this->transform = transform;
	}
	bool is_valid() override { return m != nullptr; }

	void execute() {
		auto ent = eng->spawn_entity_class<StaticMeshEntity>();
		ent->Mesh->set_model(m);
		ent->set_ws_transform(transform);
		ent->editor_name = strip_extension(m->get_name());
		handle = ent->self_id.handle;
		ed_doc.on_node_created.invoke(handle);
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		ed_doc.on_node_will_delete.invoke(handle);
		eng->remove_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = 0;
	}
	std::string to_string() override {
		return "CreateStaticMeshCommand";
	}

	uint64_t handle = 0;
	Model* m{};
	glm::mat4 transform;
};
class CreateCppClassCommand : public Command
{
public:
	CreateCppClassCommand(const std::string& cppclassname, const glm::mat4& transform) {
		ti = ClassBase::find_class(cppclassname.c_str());
		this->transform = transform;
	}
	bool is_valid() override { return ti != nullptr; }

	void execute() {
		auto ent = eng->spawn_entity_from_classtype(ti);
		ent->set_ws_transform(transform);
		ent->editor_name = ent->get_type().classname;
		handle = ent->self_id.handle;
		ed_doc.on_node_created.invoke(handle);
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		ed_doc.on_node_will_delete.invoke(handle);
		eng->remove_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = 0;
	}
	std::string to_string() override {
		return "CreateCppClassCommand";
	}
	const ClassTypeInfo* ti = nullptr;
	glm::mat4 transform;
	uint64_t handle = 0;
};

class PropertyChangeCommand
{
public:
	PropertyChangeCommand(std::string field, std::vector<ClassBase*> classes);
	void commit_post_change();
private:
	struct Change {
		ClassBase* instance = nullptr;
		std::string field_name;
		std::string pre_serialized_string;
		std::string post_serialized_string;
	};
	std::vector<Change> changes;
	std::vector<std::string> fields;
};

class RemoveEntitiesCommand : public Command
{
public:
	RemoveEntitiesCommand(std::vector<uint64_t> handles) {
		std::vector<Entity*> ents;
		for (auto h : handles) {
			ents.push_back(eng->get_entity(h));
		}
		serialized_str = LevelSerialization::serialize_entities_to_string(ents);

		this->handles = handles;
	}

	void execute() {
		for (auto h : handles) {
			ed_doc.on_node_will_delete.invoke(h);
			eng->remove_entity(eng->get_entity(h));
		}
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		auto all_ents = LevelSerialization::unserialize_entities_from_string(serialized_str);
		eng->get_level()->insert_unserialized_entities_into_level(all_ents, false/* keep ids*/);
		for (auto e : all_ents)
			ed_doc.on_node_created.invoke(e->self_id.handle);
		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "RemoveEntitiesCommand";
	}

	std::string serialized_str;
	std::vector<uint64_t> handles;
};




static float alphadither = 0.0;
void menu_temp()
{
	ImGui::SliderFloat("alpha", &alphadither, 0.0, 1.0);
}




// check every 5 seconds
ConfigVar g_assetbrowser_reindex_time("g_assetbrowser_reindex_time", "5.0", CVAR_FLOAT | CVAR_UNBOUNDED);


// Unproject mouse coords into a vector, cast that into the world via physics
glm::vec3 EditorDoc::unproject_mouse_to_ray(const int mx, const int my)
{
	Ray r;
	// get ui size
	const auto size = gui->ws_position;
	const int wx = gui->ws_size.x;
	const int wy = gui->ws_size.y;
	const float aratio = float(wy) / wx;
	glm::vec3 ndc = glm::vec3(float(mx - size.x) / wx, float(my - size.y) / wy, 0);
		ndc = ndc * 2.f - 1.f;
		ndc.y *= -1;
	
	if (using_ortho) {
		glm::vec3 pos = ortho_camera.position - ortho_camera.side * ndc.x * ortho_camera.width + ortho_camera.up * ndc.y * ortho_camera.width * aratio;
		glm::vec3 front = ortho_camera.front;
		r.pos = pos;
		r.dir = front;
	}
	else {
		r.pos = vs_setup.origin;


		glm::mat4 invviewproj = glm::inverse(vs_setup.viewproj);
		glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
		point /= point.w;

		glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

		r.dir = dir;
	}
	return r.dir;
}

Color32 to_color32(glm::vec4 v) {
	Color32 c;
	c.r = glm::clamp(v.r * 255.f,0.f,255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}



void EditorDoc::draw_menu_bar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) {
				open("");
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {
				open_the_open_popup();

			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
			}

			ImGui::EndMenu();
		}
		
		ImGui::EndMenuBar();
	}

}

void EditorDoc::init()
{
	//ed_schema.load("./Data/classes.txt");
	global_asset_browser.init();
}

bool EditorDoc::save_document_internal()
{
	sys_print("*** saving map document\n");

	std::string str = LevelSerialization::serialize_level(eng->get_level());
	
	std::ofstream outfile(get_save_root_dir() + get_doc_name());
	outfile.write(str.c_str(), str.size());
	outfile.close();

	return true;
}

void EditorDoc::on_map_load_return(bool good)
{
	if (!good) {
		eng->open_level("__empty__");
		// this will call on_map_load_return again, sort of an infinite loop risk, but should always be valid with "__empty__"
	}
	else {
		set_doc_name(eng->get_level()->get_name());

		if (is_editing_a_schema) {
			if(schema_source)
				eng->spawn_entity_schema(schema_source);
		}

		on_start.invoke();
	}
}
void EditorDoc::open_document_internal(const char* levelname, const char* arg)
{
	// schema vs level edit switch
	if (strcmp(arg, "schema") == 0)
		is_editing_a_schema = true;
	else
		is_editing_a_schema = false;

	id_start = 0;

	if (!is_editing_a_schema) {
		bool needs_new_doc = true;
		if (strlen(levelname) != 0) {
			set_doc_name(levelname);
			eng->open_level(levelname);	// queues load
			needs_new_doc = false;
		}

		if (needs_new_doc) {
			sys_print("creating new document\n");
			set_empty_name();
			eng->open_level("__empty__");	// queues load
		}
	}
	else {
		// flow: tell engine to open an empty level
		// after succeding, add the schema in the callback
		schema_source = g_schema_loader.load_schema(levelname);
		set_doc_name(levelname);
		eng->open_level("__empty__");
	}
	eng_local.on_map_load_return.add(this, &EditorDoc::on_map_load_return);
	is_open = true;

	assert(!gui->parent);
	eng->get_gui()->add_gui_panel_to_root(gui.get());
	eng->get_gui()->set_focus_to_this(gui.get());
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  leveldock.ini");
}

void EditorDoc::close_internal()
{
	eng_local.on_map_load_return.remove(this);

	// level will get unloaded in the main loop

	sys_print("*** deleting map file for editor...\n");

	command_mgr->clear_all();
	
	on_close.invoke();

	gui->unlink_and_release_from_parent();

	is_open = false;
}

void ManipulateTransformTool::on_key_down(const SDL_KeyboardEvent& key)
{
	if (eng->is_game_focused() || !ed_doc.selection_state->has_any_selected())
		return;
	uint32_t scancode = key.keysym.scancode;
	bool has_shift = key.keysym.mod & KMOD_SHIFT;
	if (scancode == SDL_SCANCODE_R) {
		if (operation_mask == ImGuizmo::ROTATE)
			swap_mode();
		else
			operation_mask = ImGuizmo::ROTATE;
	}
	else if (scancode == SDL_SCANCODE_G) {
		if (operation_mask == ImGuizmo::TRANSLATE)
			swap_mode();
		else
			operation_mask = ImGuizmo::TRANSLATE;
	}
	else if (scancode == SDL_SCANCODE_S) {
		operation_mask = ImGuizmo::SCALE;
		mode = ImGuizmo::LOCAL;	// local scaling only
	}


	else if (scancode == SDL_SCANCODE_LEFTBRACKET) {
		if (operation_mask == ImGuizmo::TRANSLATE) {
			translation_snap = translation_snap * 2.0;
		}
		else if (operation_mask == ImGuizmo::SCALE) {
			translation_snap = translation_snap * 2.0;
		}
	}
	else if (scancode == SDL_SCANCODE_RIGHTBRACKET) {
		if (operation_mask == ImGuizmo::TRANSLATE) {
			translation_snap = translation_snap * 0.5;
		}
		else if (operation_mask == ImGuizmo::SCALE) {
			translation_snap = translation_snap * 9.5;
		}
	}
}
void EditorDoc::on_mouse_down(int x, int y, int button)
{

	if (selection_state->has_any_selected() && (manipulate->is_hovered() || manipulate->is_using()))
		return;

	if (button == 1) {
		auto handle = idraw->mouse_pick_scene_for_editor(x, y);

		if (handle.is_valid()) {
			auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
			if (component_ptr && component_ptr->get_owner()) {
				selection_state->set_select_only_this(component_ptr->get_owner()->self_id);
			}
		}

	}
}

void EditorDoc::on_key_down(const SDL_KeyboardEvent& key)
{
	uint32_t scancode = key.keysym.scancode;
	const float ORTHO_DIST = 20.0;
	if (scancode == SDL_SCANCODE_DELETE) {
		if (selection_state->has_any_selected() && !selection_state->get_ec_selected()) {
			std::vector<uint64_t> handles;
			auto& s = selection_state->get_selection();
			for (auto e : s) handles.push_back(e.handle);
			RemoveEntitiesCommand* cmd = new RemoveEntitiesCommand(handles);
			command_mgr->add_command(cmd);
		}
	}
	else if (scancode == SDL_SCANCODE_KP_5) {
		using_ortho = false;
	}
	else if (scancode == SDL_SCANCODE_KP_7 && key.keysym.mod & KMOD_CTRL) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(0, ORTHO_DIST, 0), glm::vec3(0, -1, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_7) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(0, -ORTHO_DIST, 0), glm::vec3(0, 1, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_3 && key.keysym.mod & KMOD_CTRL) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(ORTHO_DIST, 0, 0), glm::vec3(-1, 0, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_3) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(-ORTHO_DIST, 0, 0), glm::vec3(1, 0, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_1 && key.keysym.mod & KMOD_CTRL) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(0, 0, ORTHO_DIST), glm::vec3(0, 0, -1));
	}
	else if (scancode == SDL_SCANCODE_KP_1) {
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.position + glm::vec3(0, 0, -ORTHO_DIST), glm::vec3(0, 0, 1));
	}
}


Bounds transform_bounds(glm::mat4 transform, Bounds b)
{
	glm::vec3 corners[8];
	corners[0] = glm::vec3(b.bmin);
	corners[1] = glm::vec3(b.bmax.x, b.bmin.y, b.bmin.z);
	corners[2] = glm::vec3(b.bmax.x, b.bmax.y, b.bmin.z);
	corners[3] = glm::vec3(b.bmin.x, b.bmax.y, b.bmin.z);

	corners[4] = glm::vec3(b.bmin.x, b.bmin.y, b.bmax.z);
	corners[5] = glm::vec3(b.bmax.x, b.bmin.y, b.bmax.z);
	corners[6] = glm::vec3(b.bmax.x, b.bmax.y, b.bmax.z);
	corners[7] = glm::vec3(b.bmin.x, b.bmax.y, b.bmax.z);
	for (int i = 0; i < 8; i++) {
		corners[i] = transform * glm::vec4(corners[i], 1.0f);
	}

	Bounds out;
	out.bmin = corners[0];
	out.bmax = corners[0];
	for (int i = 1; i < 8; i++) {
		out.bmax = glm::max(out.bmax, corners[i]);
		out.bmin = glm::min(out.bmin, corners[i]);
	}
	return out;
}

void some_funcs()
{
	auto gedlayout = ed_doc.gui.get();
	ImGui::DragInt2("box pos", &gedlayout->test->ls_position.x, 1.f, -1000, 1000);
	auto& a = gedlayout->test->anchor;
	int x[2] = { a.positions[0][0],a.positions[1][1] };
	ImGui::SliderInt2("anchor", x, 0, 255);
	a.positions[0][0] = x[0];
	a.positions[0][1] = x[0];
	a.positions[1][0] = x[1];
	a.positions[1][1] = x[1];
}
AddToDebugMenu myfuncs("edbox test", some_funcs);

void EditorDoc::tick(float dt)
{

	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x=0, y=0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			if (using_ortho)
				ortho_camera.update_from_input(eng->get_input_state()->keys, x, y, aratio);
			else
				camera.update_from_input(eng->get_input_state()->keys, x, y, glm::mat4(1.f));
		}
	}

	if(!using_ortho)
		vs_setup = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
	else {
		View_Setup vs;
		vs.far = 100.0;
		vs.front = ortho_camera.front;
		vs.origin = ortho_camera.position;
		vs.height = window_sz.y;
		vs.width = window_sz.x;
		vs.proj = ortho_camera.get_proj_matrix(aratio);
		vs.view = ortho_camera.get_view_matrix();
		vs.viewproj = vs.proj * vs.view;
		vs.near = 0.001;
		vs.fov = glm::radians(90.f);
		vs_setup = vs;
	}
}

bool line_plane_intersect(Ray r, glm::vec3 plane, float planed, glm::vec3& intersect)
{
	float denom = dot(plane, r.dir);

	if (abs(denom) > 0.00001) {	// such a high epsilon to deal with weird issues
		float planedist = dot(plane, r.pos) + planed;
		float time = -planedist / denom;
		intersect = r.pos + r.dir * time;
		return true;
	}
	return false;
}

glm::vec3 project_onto_line(glm::vec3 a, glm::vec3 b, glm::vec3 p)
{
	glm::vec3 ap = p - a;
	glm::vec3 ab = b - a;
	return a + dot(ap, ab) / dot(ab, ab) * ab;
}

void EditorDoc::transform_tool_update()
{
	glm::vec3 planes[3];
	float planed[3];

	planes[0] = glm::vec3(1, 0, 0);
	planes[1] = glm::vec3(0, 1, 0);
	planes[2] = glm::vec3(0, 0, 1);
	for (int i = 0; i < 3; i++) {
		planed[i] = -glm::dot(planes[i], transform_tool_origin);
	}

	// now find the intersection point
	bool xaxis = axis_bit_mask & 1;
	bool yaxis = axis_bit_mask & (1<<1);
	bool zaxis = axis_bit_mask & (1<<2);
	bool xandy = xaxis && yaxis;
	bool yandz = yaxis && zaxis;
	bool zandx = zaxis && xaxis;

	#if  0
	Ray r;
	cast_ray_into_world(&r);	// just using this to get the unprojected ray :/
	bool good2 = true;
	glm::vec3 intersect_point=glm::vec3(0.f);
	if (xandy) {
		bool good = line_plane_intersect(r, planes[0], planed[0], intersect_point);
		if (!good) line_plane_intersect(r, planes[1], planed[1], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[2], intersect_point);
	}
	else if (yandz) {
		bool good = line_plane_intersect(r, planes[1], planed[1], intersect_point);
		if (!good) line_plane_intersect(r, planes[2], planed[2], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[0], intersect_point);
	}
	else if (zandx) {
		bool good = line_plane_intersect(r, planes[2], planed[2], intersect_point);
		if (!good) line_plane_intersect(r, planes[0], planed[0], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[1], intersect_point);
	}
	else if (xaxis) {
		good2 = line_plane_intersect(r, planes[0], planed[0], intersect_point);
	}
	else if (yaxis) {
		good2 = line_plane_intersect(r, planes[1], planed[1], intersect_point);
	}
	else if (zaxis) {
		good2 = line_plane_intersect(r, planes[2], planed[2], intersect_point);
	}
#endif

	//if(good2)
	//	selected_node->position = intersect_point;

}


void EditorDoc::overlay_draw()
{
	static MeshBuilder mb;
	mb.Begin();
	mb.PushLineBox(glm::vec3(-1), glm::vec3(1), COLOR_BLUE);
	if (selection_state->has_any_selected()) {
		Bounds total_bounds;
		auto& selected = selection_state->get_selection();
		for (auto& s : selected) {
			//Model* m = s->get_rendering_model();
			//if (m) {
			//	auto transform = s->get_transform();
			//	total_bounds = bounds_union(total_bounds,transform_bounds(transform,m->get_bounds()));
			//}
		}
		mb.PushLineBox(total_bounds.bmin, total_bounds.bmax, COLOR_RED);
	}
	mb.End();
	mb.Draw(GL_LINES);
}

uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}


#include <glm/gtc/type_ptr.hpp>
bool ManipulateTransformTool::is_hovered()
{
	return ImGuizmo::IsOver();
}
bool ManipulateTransformTool::is_using()
{
	return ImGuizmo::IsUsing();
}

static void decompose_transform(const glm::mat4& transform, glm::vec3& p, glm::quat& q, glm::vec3& s)
{
	s = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	q = glm::normalize(glm::quat_cast(transform));
	p = transform[3];
}


ManipulateTransformTool::ManipulateTransformTool()
{
	ed_doc.post_node_changes.add(this, &ManipulateTransformTool::on_entity_changes);
	ed_doc.on_component_deleted.add(this, &ManipulateTransformTool::on_component_deleted);
	ed_doc.selection_state->on_selection_changed.add(this,
		&ManipulateTransformTool::on_selection_changed);
	ed_doc.on_close.add(this, &ManipulateTransformTool::on_close);
	ed_doc.on_start.add(this, &ManipulateTransformTool::on_open);

	ed_doc.selection_state->on_selection_changed.add(this, &ManipulateTransformTool::on_selection_changed);

	// refresh cached data
	ed_doc.prop_editor->on_property_change.add(this, &ManipulateTransformTool::on_selection_changed);

	ed_doc.gui->key_down_delegate.add(this, &ManipulateTransformTool::on_key_down);
}

void ManipulateTransformTool::on_close() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_open() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_component_deleted(EntityComponent* ec) {
	update_pivot_and_cached();
}
void ManipulateTransformTool::on_entity_changes() {
	update_pivot_and_cached();
}
void ManipulateTransformTool::on_selection_changed() {
	update_pivot_and_cached();
}

void ManipulateTransformTool::update_pivot_and_cached()
{
	world_space_of_selected.clear();
	auto& ss = ed_doc.selection_state;
	if (ss->is_selecting_entity_component()) {
		auto ec = ss->get_ec_selected();
		world_space_of_selected.push_back(ec->get_ws_transform());
	}
	else if (ss->has_any_selected()) {
		for (auto e : ss->get_selection()) {
			if(e.get())
				world_space_of_selected.push_back(e.get()->get_ws_transform());
		}
	}

	if (world_space_of_selected.size() == 1) {
		pivot_transform = world_space_of_selected[0];
	}
	else if (world_space_of_selected.size() > 1) {
		glm::vec3 v = glm::vec3(0.f);
		for (int i = 0; i < world_space_of_selected.size(); i++) {
			v += glm::vec3(world_space_of_selected[i][3]);
		}
		v /= (float)world_space_of_selected.size();
		pivot_transform = glm::translate(glm::mat4(1), v);
	}
	current_transform_of_group = pivot_transform;

	if (world_space_of_selected.size() == 0)
		state = IDLE;
	else
		state = SELECTED;
}

void ManipulateTransformTool::on_selected_tarnsform_change(uint64_t h) {
	update_pivot_and_cached();
}

void ManipulateTransformTool::begin_drag() {
	ASSERT(state == SELECTED);
	state = MANIPULATING_OBJS;
}

void ManipulateTransformTool::end_drag() {
	ASSERT(state == MANIPULATING_OBJS);
	update_pivot_and_cached();
}

void ManipulateTransformTool::update()
{
	if (state == IDLE)
		return;

	//auto selected = ed_doc.selection_state.sel

	const float* view = glm::value_ptr(ed_doc.vs_setup.view);

	const glm::mat4 friendly_proj_matrix = ed_doc.vs_setup.make_opengl_perspective_with_near_far();
	const float* proj = glm::value_ptr(friendly_proj_matrix);

	float* model = glm::value_ptr(current_transform_of_group);

	ImGuizmo::SetImGuiContext(eng->get_imgui_context());
	ImGuizmo::SetDrawlist();
	const auto s_pos = ed_doc.gui->ws_position;
	const auto s_sz = ed_doc.gui->ws_size;

	ImGuizmo::SetRect(s_pos.x, s_pos.y, s_sz.x, s_sz.y);
	ImGuizmo::Enable(true);
	ImGuizmo::SetOrthographic(ed_doc.using_ortho);

	glm::vec3 snap(-1);
	if (operation_mask == ImGuizmo::TRANSLATE && has_translation_snap)
		snap = glm::vec3(translation_snap);
	else if (operation_mask == ImGuizmo::SCALE && has_scale_snap)
		snap = glm::vec3(scale_snap);
	else if (operation_mask == ImGuizmo::ROTATE&&has_rotation_snap)
		snap = glm::vec3(rotation_snap);


	bool good = ImGuizmo::Manipulate(view, proj, operation_mask, mode, model,nullptr,(snap.x>0)?&snap.x:nullptr);
	
	if (ImGuizmo::IsOver() && state == SELECTED && ImGui::GetIO().MouseClicked[2]) {
		ImGui::OpenPopup("manipulate tool menu");
	}
	if (ImGui::BeginPopup("manipulate tool menu")) {
		ImGui::Checkbox("T Snap", &has_translation_snap);
		ImGui::Checkbox("R Snap", &has_rotation_snap);
		ImGui::Checkbox("S Snap", &has_scale_snap);

		ImGui::EndPopup();
	}
	
	bool create_command = false;
	if (ImGuizmo::IsUsingAny() && state == SELECTED) {
		begin_drag();
	}
	else if (!ImGuizmo::IsUsingAny() && state == MANIPULATING_OBJS) {
		end_drag();
	}
	if (state == MANIPULATING_OBJS) {
		// save off
		auto& ss = ed_doc.selection_state;
		if (ss->get_ec_selected()) {
			glm::mat4 ws =  current_transform_of_group;
			ss->get_ec_selected()->set_ws_transform(ws);
		}
		else {
			auto& arr = ss->get_selection();
			for (int i = 0; i < arr.size(); i++) {
				glm::mat4 ws = current_transform_of_group * glm::inverse(pivot_transform) * world_space_of_selected[i];
				arr[i].get()->set_ws_transform(ws);
			}
		}
	}
}

void EditorDoc::imgui_draw()
{
	outliner->draw();

	prop_editor->draw();

	IEditorTool::imgui_draw();
}

void EditorDoc::hook_scene_viewport_draw()
{
	//if (get_focus_state() != editor_focus_state::Focused)
	//	return;

	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
		{
			glm::mat4 drop_transform = glm::mat4(1.f);

			int x, y;
			SDL_GetMouseState(&x, &y);
			auto size = gui->ws_position;
			const float scene_depth = idraw->get_scene_depth_for_editor(x-size.x, y-size.y);

			glm::vec3 dir = unproject_mouse_to_ray(x, y);
			glm::vec3 worldpos = (abs(scene_depth) > 30.0) ? vs_setup.origin + dir * 5.0f : vs_setup.origin + dir * scene_depth;
			drop_transform[3] = glm::vec4(worldpos, 1.0);

			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			if (resource->type->get_type_name() == "Entity (C++)") {
				command_mgr->add_command(new CreateCppClassCommand(
					resource->filename, 
					drop_transform)
				);
			}
			else if (resource->type->get_type_name() == "Model") {
				command_mgr->add_command(new CreateStaticMeshCommand(
					resource->filename, 
					drop_transform)
				);
			}
			else if (resource->type->get_type_name() == "Schema") {
				command_mgr->add_command(new CreateSchemaCommand(
					resource->filename,
					drop_transform)
				);
			}
		}
		ImGui::EndDragDropTarget();
	}

	manipulate->update();


}

const View_Setup& EditorDoc::get_vs()
{
	return vs_setup;
}


ObjectOutliner::ObjectOutliner()
{
	ed_doc.on_close.add(this, &ObjectOutliner::on_close);
	ed_doc.on_start.add(this, &ObjectOutliner::on_start);
	ed_doc.post_node_changes.add(this, &ObjectOutliner::on_changed_ents);
	ed_doc.on_change_name.add(this, &ObjectOutliner::on_change_name);
}
void ObjectOutliner::draw_table_R(Node* n, int depth)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::Dummy(ImVec2(depth*5.f, 0));
	ImGui::SameLine();

	ImGui::PushID(n);
	{
		const bool item_is_selected = ed_doc.selection_state->is_node_selected({ n->handle });
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			if (n->handle != 0) {
				if(ImGui::GetIO().KeyShift)
					ed_doc.selection_state->add_to_selection({ n->handle });
				else
					ed_doc.selection_state->set_select_only_this({ n->handle });
			}
			else
				ed_doc.selection_state->clear_all_selected();
		}
	}
	
	ImGui::SameLine();

	if (n->handle == 0) {
		ImGui::Text(ed_doc.get_full_output_path().c_str());
	}
	else {
		auto entity = eng->get_entity(n->handle);
		ImGui::Text(entity->editor_name.c_str());
	}

	for (int i = 0; i < n->children.size(); i++)
		draw_table_R(n->children[i], depth + 1);

	ImGui::PopID();
}

void ObjectOutliner::draw()
{

	if (!ImGui::Begin("Outliner")) {
		ImGui::End();

		return;
	}
	ImGuiTableFlags const flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
	//if (ImGui::Begin("PropEdit")) {
	if (ImGui::BeginTable("Table", 1, flags)) {
		ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);

		draw_table_R(rootnode, 0);

		ImGui::EndTable();
	}
	ImGui::End();
}

void EdPropertyGrid::draw_components_R(EntityComponent* ec, float ofs)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::PushID(ec);

	ImGuiSelectableFlags selectable_flags =  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
	if (ImGui::Selectable("##selectednode", ec == ed_doc.selection_state->get_ec_selected(), selectable_flags, ImVec2(0, 0))) {
		on_select_component(ec);
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		dragging_component = ec;

		ImGui::SetDragDropPayload("SetComponentParent",nullptr, 0);

		ImGui::Text("Set parent of: %s", dragging_component->eSelfNameString.c_str());

		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SetComponentParent");
		if (payload) {
			if (dragging_component != dragging_component->get_owner()->get_root_component() && ec != dragging_component) {
				dragging_component->attach_to_parent(ec);
			}
		}
		ImGui::EndDragDropTarget();
	}


	ImGui::SameLine();
	ImGui::Dummy(ImVec2(ofs,1.0));
	ImGui::SameLine();
	ImGui::Text(ec->eSelfNameString.c_str());

	for (int i = 0; i < ec->children.size(); i++) {
		draw_components_R(ec->children[i], ofs + 10.0);
	}

	ImGui::PopID();
}

void set_component_default_name(EntityComponent* ec)
{
	std::unordered_set<std::string> names;
	auto parent = ec->get_owner();
	for (int i = 0; i < parent->get_all_components().size(); i++) {
		names.insert(parent->get_all_components()[i]->eSelfNameString);
	}
	std::string wantname = ec->get_type().classname;
	std::string testname = wantname;
	int number = 2;
	while (names.find(testname) != names.end())
		testname = wantname + std::to_string(number++);
	ec->eSelfNameString = testname;
}

void EdPropertyGrid::draw()
{
	if (ImGui::Begin("Properties")) {
		if (ed_doc.selection_state->has_any_selected()) {
			grid.update();


			if (grid.rows_had_changes) {
				auto& ss = ed_doc.selection_state;
				if (ss->is_selecting_entity_component()) {
					auto c = ss->get_ec_selected();
					c->post_change_transform_R();	// for good measure, call this, updates stuff if the transform was the changed property
					c->editor_on_change_property();
				}
				else if (ss->num_entities_selected() == 1) {
					auto e = ss->get_selection()[0].get();
					assert(e->get_root_component());
					e->get_root_component()->post_change_transform_R();
					e->get_root_component()->editor_on_change_property();
				}

				on_property_change.invoke();

			}

		}
		else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Components")) {

		if (!ed_doc.selection_state->has_any_selected()) {
			ImGui::Text("Nothing selected\n");
		}
		else if (ed_doc.selection_state->num_entities_selected() != 1 && !ed_doc.selection_state->get_ec_selected()) {
			ImGui::Text("Select 1 entity to see components\n");
		}
		else {

			Entity* ent = nullptr;
			if (ed_doc.selection_state->get_ec_selected())
				ent = ed_doc.selection_state->get_ec_selected()->get_owner();
			else
				ent = eng->get_entity(ed_doc.selection_state->get_selection()[0].handle);

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("addcomponentpopup");
			}
			if (ImGui::BeginPopup("addcomponentpopup")) {
				auto iter = ClassBase::get_subclasses<EntityComponent>();
				for (; !iter.is_end(); iter.next()) {
					if (ImGui::Selectable(iter.get_type()->classname)) {
						auto ec = ent->create_and_attach_component_type(iter.get_type(), nullptr);

						set_component_default_name(ec);

						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndPopup();
			}

			uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
				ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
			if (ImGui::BeginTable("animadfedBrowserlist", 1, ent_list_flags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Color32{ 59, 0, 135 }.to_uint());
				ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
				if (ImGui::Selectable("##selectednode", ed_doc.selection_state->get_ec_selected()==nullptr, selectable_flags, ImVec2(0, 0))) {
					if (ed_doc.selection_state->get_ec_selected() != nullptr)
						ed_doc.selection_state->set_select_only_this(ed_doc.selection_state->get_ec_selected()->get_owner()->self_id);
				}

				ImGui::SameLine();
				ImGui::Text(ent->get_type().classname);
				auto& array = ent->get_all_components();
				for (int row_n = 0; row_n < array.size(); row_n++)
				{
					auto& res = array[row_n];		
					if(res.get()->attached_parent.get()==nullptr)
						draw_components_R(res.get(), 8.0);
				}
				ImGui::EndTable();
			}
		}


	}
	ImGui::End();

	if (wants_set_component) {
		ed_doc.selection_state->set_entity_component_select(set_this_component);
		set_this_component = nullptr;
		wants_set_component = false;
	}

}

#include "Assets/AssetLoaderRegistry.h"

class AssetPropertyEditor : public IPropertyEditor
{
public:
	virtual bool internal_update() {
		if (!has_init) {
			IAsset** ptr_to_asset = (IAsset**)prop->get_ptr(instance);
			has_init = true;
			if(*ptr_to_asset)
				asset_str = (*ptr_to_asset)->get_name();
			metadata = AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class(prop->range_hint));
			loader = AssetLoaderRegistry::get().get_loader_for_type_name(prop->range_hint);
		}
		if (!metadata) {
			ImGui::Text("Asset has no metadata: %s\n", prop->range_hint);
			return false;
		}
		if (!loader) {
			ImGui::Text("Asset has no loader: %s\n", prop->range_hint);
			return false;
		}

		auto drawlist = ImGui::GetWindowDrawList();
		auto& style = ImGui::GetStyle();
		auto min = ImGui::GetCursorScreenPos();
		auto sz = ImGui::CalcTextSize(asset_str.c_str());
		float width = ImGui::CalcItemWidth();
		Color32 color = metadata->get_browser_color();
		color.r *= 0.4;
		color.g *= 0.4;
		color.b *= 0.4;

		drawlist->AddRectFilled(ImVec2(min.x - style.FramePadding.x * 0.5f, min.y), ImVec2(min.x + width, min.y + sz.y + style.FramePadding.y * 2.0), 
			color.to_uint());
		auto cursor = ImGui::GetCursorPos();
		ImGui::Text(asset_str.c_str());
		ImGui::SetCursorPos(cursor);
		ImGui::InvisibleButton("##adfad", ImVec2(width, sz.y + style.FramePadding.y * 2.f));
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			ImGui::BeginTooltip();
			ImGui::Text(string_format("Drag and drop %s asset here", metadata->get_type_name().c_str()));
			ImGui::EndTooltip();
		}
		bool ret = false;
		if (ImGui::BeginDragDropTarget())
		{
			//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
			//if (payload->IsDataType("AssetBrowserDragDrop"))
			//	sys_print("``` accepting\n");

			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
			if (payload) {

				AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
				bool actually_accept = false;
				if (resource->type == metadata) {
					actually_accept = true;
				}

				if (actually_accept) {
					if (payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
					{
						IAsset** ptr_to_asset = (IAsset**)prop->get_ptr(instance);
						*ptr_to_asset = loader->load_asset(resource->filename);
						
						if (*ptr_to_asset)
							asset_str = (*ptr_to_asset)->get_name();
						else
							asset_str = "";


						ret = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		return ret;

	}
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return !asset_str.empty(); }
	virtual void reset_value() {
		asset_str = "";
		auto ptr = (IAsset**)prop->get_ptr(instance);
		*ptr = nullptr;
	}
private:
	bool has_init = false;
	std::string asset_str;
	const AssetMetadata* metadata = nullptr;
	IAssetLoader* loader = nullptr;
};

ADDTOFACTORYMACRO_NAME(AssetPropertyEditor, IPropertyEditor, "AssetPtr");

class ColorEditor : public IPropertyEditor
{
public:
	virtual bool internal_update() {
		assert(prop->type == core_type_id::Int32);
		Color32* c = (Color32*)prop->get_ptr(instance);
		ImVec4 col = ImGui::ColorConvertU32ToFloat4(c->to_uint());
		if (ImGui::ColorEdit3("##coloredit", &col.x)) {
			auto uint_col = ImGui::ColorConvertFloat4ToU32(col);
			uint32_t* prop_int = (uint32_t*)prop->get_ptr(instance);
			*prop_int = uint_col;
			return true;
		}
		return false;
	}
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { 
		Color32* c = (Color32*)prop->get_ptr(instance);
		return c->r != 255 || c->g != 255 || c->b != 255;
	
	}
	virtual void reset_value() {
		Color32* c = (Color32*)prop->get_ptr(instance);
		*c = COLOR_WHITE;
	}
private:
};

ADDTOFACTORYMACRO_NAME(ColorEditor, IPropertyEditor, "ColorUint");

EdPropertyGrid::EdPropertyGrid()
{
	auto& ss = ed_doc.selection_state;
	ss->on_selection_changed.add(this, &EdPropertyGrid::refresh_grid);
	ed_doc.post_node_changes.add(this, &EdPropertyGrid::refresh_grid);
	ed_doc.on_close.add(this, &EdPropertyGrid::on_close);
	ed_doc.on_component_deleted.add(this, &EdPropertyGrid::on_ec_deleted);
}

void EdPropertyGrid::refresh_grid()
{
	grid.clear_all();
	
	auto& ss = ed_doc.selection_state;

	if (!ss->has_any_selected())
		return;


	if(ss->is_selecting_entity_component()) {
		auto c = ss->get_ec_selected();
		auto ti = &c->get_type();
		while (ti) {
			if (ti->props)
				grid.add_property_list_to_grid(ti->props, c);
			ti = ti->super_typeinfo;
		}
	}
	else if(ss->num_entities_selected()==1){
		auto entity = eng->get_entity(ss->get_selection()[0].handle);
		auto ti = &entity->get_type();
		while (ti) {
			if (ti->props) {
				grid.add_property_list_to_grid(ti->props, entity);
			}
			ti = ti->super_typeinfo;
		}


		auto c = entity->get_root_component();
		ASSERT(c);
		ti = &c->get_type();
		while (ti) {
			if (ti->props)
				grid.add_property_list_to_grid(ti->props, c);
			ti = ti->super_typeinfo;
		}
	}
}


SelectionState::SelectionState()
{
	ed_doc.on_node_will_delete.add(this, &SelectionState::on_node_deleted);
	ed_doc.on_close.add(this, &SelectionState::on_close);
	ed_doc.on_component_deleted.add(this, &SelectionState::on_entity_component_delete);
}

EntityNameDatabase_Ed::EntityNameDatabase_Ed()
{
	ed_doc.on_node_created.add(this, &EntityNameDatabase_Ed::on_add);
	ed_doc.on_node_will_delete.add(this, &EntityNameDatabase_Ed::on_delete);
	ed_doc.on_start.add(this, &EntityNameDatabase_Ed::on_start);
	ed_doc.on_close.add(this, &EntityNameDatabase_Ed::on_close);

	ed_doc.prop_editor->on_property_change.add(this, &EntityNameDatabase_Ed::on_property_change);
}
void EntityNameDatabase_Ed::invoke_change_name(uint64_t h)
{
	ed_doc.on_change_name.invoke(h);
}

DECLARE_ENGINE_CMD(STRESS_TEST)
{
	auto model = mods.find_or_load("cube.cmdl");
	for (int z = 0; z < 20; z++) {
		for (int y = 0; y < 20; y++) {
			for (int x = 0; x < 20; x++) {
				glm::vec3 p(x, y, z);
				glm::mat4 transform = glm::translate(glm::mat4(1), p*2.0f);

				auto ent = eng->spawn_entity_class<StaticMeshEntity>();
				ent->Mesh->set_model(model);
				ent->set_ws_transform(transform);
				
			}
		}
	}
}
#include "Render/MaterialPublic.h"
DECLARE_ENGINE_CMD(STRESS_TEST_DECAL)
{
	for (int z = 0; z < 10; z++) {
		for (int y = 0; y < 10; y++) {
			for (int x = 0; x < 10; x++) {
				glm::vec3 p(x, y, z);
				glm::mat4 transform = glm::translate(glm::mat4(1), p * 2.0f);

				auto ent = eng->spawn_entity_class<DecalEntity>();
				ent->Decal->set_material(imaterials->find_material_instance("bulletDecal"));
				ent->set_ws_transform(transform);
			}
		}
	}
}

UndoRedoSystem::UndoRedoSystem() {
	hist.resize(HIST_SIZE, nullptr);

	ed_doc.gui->key_down_delegate.add(this, &UndoRedoSystem::on_key_event);
}
void UndoRedoSystem::on_key_event(const SDL_KeyboardEvent& key)
{
	if (key.keysym.scancode == SDL_SCANCODE_Z && key.keysym.mod & KMOD_CTRL)
		undo();
}

EditorDoc::EditorDoc() {
	gui = std::make_unique<EditorUILayout>();

	gui->key_down_delegate.add(this, &EditorDoc::on_key_down);
	gui->mouse_down_delegate.add(this, &EditorDoc::on_mouse_down);
	gui->wheel_delegate.add(this, &EditorDoc::on_mouse_wheel);

	command_mgr = std::make_unique<UndoRedoSystem>();
	selection_state = std::make_unique<SelectionState>();
	prop_editor = std::make_unique<EdPropertyGrid>();
	manipulate = std::make_unique<ManipulateTransformTool>();
	outliner = std::make_unique<ObjectOutliner>();
	database = std::make_unique<EntityNameDatabase_Ed>();
}