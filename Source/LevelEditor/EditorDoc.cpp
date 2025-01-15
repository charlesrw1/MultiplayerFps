#include "EditorDocLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "glm/gtx/euler_angles.hpp"
#include "External/ImGuizmo.h"

#include "Framework/MeshBuilder.h"

#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "Framework/DictWriter.h"

#include "Physics/Physics2.h"

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

ConfigVar g_editor_newmap_template("g_editor_newmap_template", "default_map.tmap", CVAR_DEV, "whenever a new map is created, it will use this map as a template");


class EditorUILayout : public GUIFullscreen
{
public:
	EditorUILayout() {
		
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
	void on_dragging(int x, int y) override {
		mouse_drag_delegate.invoke(x, y);
	}

	MulticastDelegate<const SDL_KeyboardEvent&> key_down_delegate;
	MulticastDelegate<const SDL_KeyboardEvent&> key_up_delegate;
	MulticastDelegate<int, int, int> mouse_down_delegate;
	MulticastDelegate<int, int> mouse_drag_delegate;

	MulticastDelegate<int, int, int> mouse_up_delegate;
	MulticastDelegate<const SDL_MouseWheelEvent&> wheel_delegate;



	GUIBox* test = nullptr;

	GUIVerticalBox* vbox = nullptr;
};




static std::string to_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}

// Create from a class, create from a schema, create from a duplication
// -> serialize to use as an interchange format


class CommandSerializeUtil
{
public:
	static std::unique_ptr<SerializedSceneFile> serialize_entities_text(std::vector<EntityPtr<Entity>> handles) {
		std::vector<Entity*> ents;
		for (auto h : handles) {
			ents.push_back(h.get());
		}
		ed_doc.validate_fileids_before_serialize();
		return std::make_unique<SerializedSceneFile>(serialize_entities_to_text(ents));
	}

};

class CreateStaticMeshCommand : public Command
{
public:
	CreateStaticMeshCommand(const std::string& modelname, const glm::mat4& transform) {

		this->transform = transform;
		this->modelname = modelname;
	}
	~CreateStaticMeshCommand() override {
	}

	bool is_valid() override { return true; }

	void execute() {
		auto ent = eng->get_level()->spawn_entity_class<StaticMeshEntity>();
		ent->set_ws_transform(transform);

		handle = ent->get_self_ptr();

		ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());

		ed_doc.on_entity_created.invoke(handle);
		ed_doc.post_node_changes.invoke();


		GetAssets().find_async<Model>(modelname.c_str(), [the_handle = ent->instance_id](GenericAssetPtr p) {
			if (p) {
				auto modelP = p.cast_to<Model>();
				
				if (modelP) {
					auto ent = eng->get_entity(the_handle);
					if (ent) {
						auto mesh_ent = ent->cast_to<StaticMeshEntity>();
						ASSERT(mesh_ent);
						mesh_ent->Mesh->set_model(modelP.get());
					}
					else
						sys_print("``` CreateStaticMeshCommand: ent handle invalid in async callback\n");
				}
			}
			});
	}
	void undo() {
		ed_doc.on_entity_will_delete.invoke(handle);
		eng->get_level()->destroy_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "CreateStaticMeshCommand";
	}

	EntityPtr<Entity> handle;
	glm::mat4 transform;
	std::string modelname{};
};
class CreateCppClassCommand : public Command
{
public:
	CreateCppClassCommand(const std::string& cppclassname, const glm::mat4& transform) {
		auto find = cppclassname.rfind('/');
		auto types = cppclassname.substr(find==std::string::npos ? 0 : find+1);
		ti = ClassBase::find_class(types.c_str());
		this->transform = transform;
	}
	bool is_valid() override { return ti != nullptr; }

	void execute() {
		assert(ti);
		auto ent = eng->get_level()->spawn_entity_from_classtype(*ti);
		ent->set_ws_transform(transform);
		ent->editor_name = ent->get_type().classname;
		handle = ent->get_self_ptr();
		ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
		ed_doc.on_entity_created.invoke(handle);
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		ed_doc.on_entity_will_delete.invoke(handle);
		eng->get_level()->destroy_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "CreateCppClassCommand";
	}
	const ClassTypeInfo* ti = nullptr;
	glm::mat4 transform;
	EntityPtr<Entity> handle;
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


class DuplicateEntitiesCommand : public Command
{
public:
	DuplicateEntitiesCommand(std::vector<EntityPtr<Entity>> handles) {

		scene = CommandSerializeUtil::serialize_entities_text(handles);
	}

	void execute() {
		auto duplicated = unserialize_entities_from_text(scene->text);

		// zero out file ids so new ones are set
		for (auto o : duplicated.get_objects())
			if(o.second->creator_source == nullptr) // ==nullptr meaning that its created by level
				o.second->unique_file_id = 0;

		eng->get_level()->insert_unserialized_entities_into_level(duplicated);


		handles.clear();
		auto& objs = duplicated.get_objects();
		for (auto& o : objs) {
			if (auto e = o.second->cast_to<Entity>())
			{
				ed_doc.on_entity_created.invoke(e->get_self_ptr());
				handles.push_back(e->get_self_ptr());
			}
		}
		ed_doc.selection_state->clear_all_selected();
		for (auto e : handles) {
			ed_doc.selection_state->add_to_entity_selection(e);
		}

		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		for (auto h : handles) {
			ed_doc.on_entity_will_delete.invoke(h);
			h->destroy();
		}

		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "DuplicateEntitiesCommand";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr<Entity>> handles;
};

class RemoveEntitiesCommand : public Command
{
public:
	RemoveEntitiesCommand(std::vector<EntityPtr<Entity>> handles) {

		scene = CommandSerializeUtil::serialize_entities_text(handles);

		this->handles = handles;
	}

	void execute() {
		for (auto h : handles) {
			ed_doc.on_entity_will_delete.invoke(h);
			h->destroy();
		}
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		auto restored = unserialize_entities_from_text(scene->text);
		eng->get_level()->insert_unserialized_entities_into_level(restored);
		auto& objs = restored.get_objects();
		for (auto& o : objs) {
			if (auto e = o.second->cast_to<Entity>())
				ed_doc.on_entity_created.invoke(e->get_self_ptr());
		}
		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "RemoveEntitiesCommand";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr<Entity>> handles;
};


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


void EditorDoc::validate_fileids_before_serialize()
{
	// first find max
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	for (auto o : objs)
		if (o->creator_source == nullptr) {
			file_id_start = std::max(file_id_start, uint32_t(o->unique_file_id));
		}
	for (auto o : objs)
		if (o->creator_source == nullptr && o->unique_file_id == 0)
			o->unique_file_id = get_next_file_id();

}

void EditorDoc::init()
{
	global_asset_browser.init();
}

bool EditorDoc::save_document_internal()
{
	sys_print("*** saving map document\n");

	auto& all_objs = eng->get_level()->get_all_objects();

	ed_doc.validate_fileids_before_serialize();

	std::vector<Entity*> all_ents;
	for (auto o : all_objs)
		if(auto e = o->cast_to<Entity>())
			all_ents.push_back(e);
	auto serialized = serialize_entities_to_text(all_ents);
	
	auto path = get_doc_name();
	auto outfile = FileSys::open_write_game(path.c_str());
	outfile->write(serialized.text.c_str(), serialized.text.size());

	sys_print("``` Wrote out to %s\n", path.c_str());

	return true;
}

void EditorDoc::on_map_load_return(bool good)
{
	if (!good) {
		eng->open_level("__empty__");
		// this will call on_map_load_return again, sort of an infinite loop risk, but should always be valid with "__empty__"
	}
	else {
		//set_doc_name(eng->get_level()->get_name());

		validate_fileids_before_serialize();

		on_start.invoke();
	}
}
bool EditorDoc::open_document_internal(const char* levelname, const char* arg)
{
	// schema vs level edit switch
	if (strcmp(arg, "schema") == 0)
		is_editing_a_schema = true;
	else
		is_editing_a_schema = false;

	file_id_start = 0;

	if (!is_editing_a_schema) {
		bool needs_new_doc = true;
		if (strlen(levelname) != 0) {
			eng->open_level(levelname);	// queues load
			needs_new_doc = false;
		}

		if (needs_new_doc) {
			// uses the newmap template to load
			const char* name = g_editor_newmap_template.get_string();
			sys_print("creating new map using template map: %s\n",name);
			set_empty_doc();


			eng->open_level(name);	// queues load
		}
	}
	else {
		// flow: tell engine to open an empty level
		// after succeding, add the schema in the callback
		schema_source = GetAssets().find_sync<Schema>(levelname).get();
		eng->open_level("__empty__");
	}
	eng_local.on_map_load_return.add(this, &EditorDoc::on_map_load_return);

	assert(!gui->parent);
	eng->get_gui()->add_gui_panel_to_root(gui.get());
	eng->get_gui()->set_focus_to_this(gui.get());
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  leveldock.ini");

	return true;
}

void EditorDoc::close_internal()
{
	eng_local.on_map_load_return.remove(this);

	// level will get unloaded in the main loop

	sys_print("*** deleting map file for editor...\n");

	command_mgr->clear_all();
	
	on_close.invoke();

	gui->unlink_and_release_from_parent();

	// close the level document, its already been saved at this point
	eng->leave_level();
}


DECLARE_ENGINE_CMD(ManipulateRotateCommand)
{

}

DECLARE_ENGINE_CMD(ManipulateTranslateCommand)
{

}

DECLARE_ENGINE_CMD(ManipulateScaleCommand)
{

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
void EditorDoc::on_mouse_drag(int x, int y)
{
	if (selection_state->has_any_selected() && (manipulate->is_hovered() || manipulate->is_using()))
		return;
	if (ImGui::GetIO().KeyShift) {
		auto handle = idraw->mouse_pick_scene_for_editor(x, y);
		if (handle.is_valid()) {
			auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
			if (component_ptr) {
				auto owner = component_ptr->get_owner();
				ASSERT(owner);
				auto ptr = owner->get_self_ptr();
				selection_state->add_to_entity_selection(ptr);
			}
		}
	}
	else if (ImGui::GetIO().KeyCtrl) {
		auto handle = idraw->mouse_pick_scene_for_editor(x, y);
		if (handle.is_valid()) {
			auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
			if (component_ptr) {
				auto owner = component_ptr->get_owner();
				ASSERT(owner);
				auto ptr = owner->get_self_ptr();
				selection_state->remove_from_selection(ptr);
			}
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
				auto owner = component_ptr->get_owner();
				ASSERT(owner);
				auto ptr = owner->get_self_ptr();

				if (ImGui::GetIO().KeyShift)
					selection_state->add_to_entity_selection(ptr);
				else if (ImGui::GetIO().KeyCtrl)
					selection_state->remove_from_selection(ptr);
				else
					selection_state->set_select_only_this(ptr);
			}
		}

	}
}

void EditorDoc::on_key_down(const SDL_KeyboardEvent& key)
{
	uint32_t scancode = key.keysym.scancode;
	bool has_shift = key.keysym.mod & KMOD_SHIFT;
	const float ORTHO_DIST = 20.0;
	if (scancode == SDL_SCANCODE_DELETE) {
		if (selection_state->has_any_selected()) {
			auto selected_handles = selection_state->get_selection_as_vector();
		
			RemoveEntitiesCommand* cmd = new RemoveEntitiesCommand(selected_handles);
			command_mgr->add_command(cmd);
		}
	}
	else if (scancode == SDL_SCANCODE_D && has_shift) {
		if (selection_state->has_any_selected()) {
			auto selected_handles = selection_state->get_selection_as_vector();;
			DuplicateEntitiesCommand* cmd = new DuplicateEntitiesCommand(selected_handles);
			command_mgr->add_command(cmd);
		}


	}

	else if (1)
		return;
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
	if (ss->has_any_selected()) {
		for (auto ehandle : ss->get_selection()) {
			EntityPtr<Entity> e = { ehandle };
			if(e.get())
				world_space_of_selected[e.handle]=(e.get()->get_ws_transform());
		}
	}
	static bool selectFirstOnly = true;
	if (world_space_of_selected.size() == 1 || (!world_space_of_selected.empty() && selectFirstOnly)) {
		pivot_transform = world_space_of_selected.begin()->second;
	}
	else if (world_space_of_selected.size() > 1) {
		glm::vec3 v = glm::vec3(0.f);
		for (auto s: world_space_of_selected) {
			v += glm::vec3(s.second[3]);
		}
		v /= (float)world_space_of_selected.size();
		pivot_transform = glm::translate(glm::mat4(1), v);
	}
	current_transform_of_group = pivot_transform;

	if (world_space_of_selected.size() == 0)
		state = IDLE;
	else
		state = SELECTED;

	auto snap_to_value = [](float x, float snap) {
		return glm::round(x / snap) * snap;
	};

	glm::vec3 p, s;
	glm::quat q;
	decompose_transform(current_transform_of_group, p, q, s);
	glm::vec3 asEuler = glm::eulerAngles(q);
	if (has_translation_snap) {
		p.x = snap_to_value(p.x, translation_snap);
		p.y = snap_to_value(p.y, translation_snap);
		p.z = snap_to_value(p.z, translation_snap);
	}
	if (has_rotation_snap) {
		float rsnap = glm::radians(rotation_snap);
		asEuler.x = snap_to_value(asEuler.x, rsnap);
		asEuler.y = snap_to_value(asEuler.y, rsnap);
		asEuler.z = snap_to_value(asEuler.z, rsnap);
	}
	current_transform_of_group = glm::translate(glm::mat4(1), p);
	current_transform_of_group = current_transform_of_group * glm::mat4_cast(glm::normalize(glm::quat(asEuler)));
	current_transform_of_group = glm::scale(current_transform_of_group, glm::vec3(s));
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

// ie: snap = base
//	on_increment()
//		snap = snap * exp
// on_decrement()
//		snap = snap / mult


ConfigVar ed_translation_snap_base("ed_translation_snap_base", "1", CVAR_FLOAT,		"editor translation snap", 0.1, 128);
ConfigVar ed_translation_snap_exp("ed_translation_snap_exp", "10", CVAR_FLOAT,		"editor translation snap", 1, 10);
ConfigVar ed_rotation_snap_base("ed_rotation_snap_base", "15", CVAR_FLOAT,			"editor rotation snap",1, 180);
ConfigVar ed_rotation_snap_exp("ed_rotation_snap_exp", "3", CVAR_FLOAT,				"editor rotation snap",1, 10);


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
	//ImGuizmo::GetStyle().TranslationLineArrowSize = 20.0;
	ImGuizmo::GetStyle().TranslationLineThickness = 6.0;
	ImGuizmo::GetStyle().RotationLineThickness = 6.0;
	ImGuizmo::GetStyle().ScaleLineThickness = 6.0;


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
		{
			auto& arr = ss->get_selection();
			for (auto elm : arr) {
				ASSERT(world_space_of_selected.find(elm) != world_space_of_selected.end());
				glm::mat4 ws = current_transform_of_group * glm::inverse(pivot_transform) * world_space_of_selected.find(elm)->second;
				EntityPtr<Entity> e = { elm };
				ASSERT(e.get());
				e.get()->set_ws_transform(ws);
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
			glm::vec3 worldpos = (abs(scene_depth) > 50.0) ? vs_setup.origin - dir * 25.0f : vs_setup.origin + dir * scene_depth;
			drop_transform[3] = glm::vec4(worldpos, 1.0);

			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			if (resource->type->get_asset_class_type()->is_a(Entity::StaticType)) {
				command_mgr->add_command(new CreateCppClassCommand(
					resource->filename, 
					drop_transform)
				);
			}
			else if (resource->type->get_asset_class_type()->is_a(Model::StaticType)) {
				command_mgr->add_command(new CreateStaticMeshCommand(
					resource->filename, 
					drop_transform)
				);
			}
	
		}
		ImGui::EndDragDropTarget();
	}

	manipulate->update();


}

ObjectOutliner::ObjectOutliner()
{
	nameFilter[0] = 0;
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
		const bool item_is_selected = ed_doc.selection_state->is_entity_selected(EntityPtr<Entity>{n->handle});
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			if (n->handle != 0) {
				if(ImGui::GetIO().KeyShift)
					ed_doc.selection_state->add_to_entity_selection({ n->handle });
				else
					ed_doc.selection_state->set_select_only_this({ n->handle });
			}
			else
				ed_doc.selection_state->clear_all_selected();
		}
		if (ImGui::IsItemHovered()&&ImGui::GetIO().MouseClicked[2]) {
			ImGui::OpenPopup("outliner_ctx_menu");
			ed_doc.selection_state->add_to_entity_selection({ n->handle });
			contextMenuHandle = n->handle;
		}
		if (ImGui::BeginPopup("outliner_ctx_menu")) {

			if (eng->get_entity(contextMenuHandle) == nullptr) {
				contextMenuHandle = 0;
				ImGui::CloseCurrentPopup();
			}
			else {
				if (ImGui::Button("Parent To This")) {
					auto me = eng->get_entity(contextMenuHandle);
					auto& ents = ed_doc.selection_state->get_selection();
					for (auto& ehandle : ents) {
						EntityPtr<Entity> ptr = { ehandle };
						if (ptr.get() == me) continue;
						auto transform = ptr->get_ws_transform();
						ptr->parent_to_entity(me);
						ptr->set_ws_transform(transform);
					}
					contextMenuHandle = 0;
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::Button("Remove Parent")) {
					auto& ents = ed_doc.selection_state->get_selection();
					for (auto& ehandle : ents) {
						EntityPtr<Entity> ptr = { ehandle };
						auto transform = ptr->get_ws_transform();
						ptr->parent_to_entity(nullptr);
						ptr->set_ws_transform(transform);
					}
					contextMenuHandle = 0;
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}
	}
	
	ImGui::SameLine();

	if (n->handle == 0) {
		ImGui::Text(ed_doc.get_full_output_path().c_str());
	}
	else {
		auto entity = eng->get_entity(n->handle);
		if (!entity->editor_name.empty())
			ImGui::Text(entity->editor_name.c_str());
		else
			ImGui::Text(entity->get_type().classname);
	}

	for (int i = 0; i < n->children.size(); i++)
		draw_table_R(n->children[i], depth + 1);

	ImGui::PopID();
}

void ObjectOutliner::draw()
{

	if (!ImGui::Begin("Outliner") || !rootnode) {
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

void EdPropertyGrid::draw_components(Entity* entity)
{
	ASSERT(selected_component != 0);
	ASSERT(eng->get_object(selected_component)->is_a<EntityComponent>());
	ASSERT(eng->get_object(selected_component)->cast_to<EntityComponent>()->entity_owner == entity);

	auto draw_component = [&](Entity* e, EntityComponent* ec) {
		ASSERT(ec && e && ec->get_owner() == e);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::PushID(ec);

		ImGuiSelectableFlags selectable_flags =  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", ec->instance_id == selected_component, selectable_flags, ImVec2(0, 0))) {
			on_select_component(ec);
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(5.f,1.0));
		ImGui::SameLine();
		ImGui::Text(ec->get_type().classname);
		ImGui::PopID();
	};

	for (auto& c : entity->get_all_components())
		if(!c->dont_serialize_or_edit)
			draw_component(entity, c);
}


void EdPropertyGrid::draw()
{
	auto& ss = ed_doc.selection_state;
	if (ImGui::Begin("Properties")) {
		if (ed_doc.selection_state->has_only_one_selected()) {
			grid.update();

			if (grid.rows_had_changes) {
			
				auto e = ss->get_only_one_selected();
				e->editor_on_change_properties();
				e->post_change_transform_R();
				
				auto ec = get_selected_component();
				if (ec)
					ec->editor_on_change_property();

				on_property_change.invoke();

			}

		}
		else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Components")) {

		if (!ss->has_any_selected()) {
			ImGui::Text("Nothing selected\n");
			selected_component = 0;
		}
		else if (!ss->has_only_one_selected()) {
			ImGui::Text("Select 1 entity to see components\n");
			selected_component = 0;
		}
		else {

			Entity* ent = ss->get_only_one_selected().get();

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("addcomponentpopup");
			}
			if (ImGui::BeginPopup("addcomponentpopup")) {
				auto iter = ClassBase::get_subclasses<EntityComponent>();
				for (; !iter.is_end(); iter.next()) {
					if (ImGui::Selectable(iter.get_type()->classname)) {
						auto ec = ent->create_and_attach_component_type(iter.get_type());
						ASSERT(ec);
						on_select_component(ec);

						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndPopup();
			}

			
			auto& comps = ent->get_all_components();
			if (comps.empty()) {
				ImGui::Text("No components. Add one above.");
			}
			else {

				uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
					ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
				if (ImGui::BeginTable("animadfedBrowserlist", 1, ent_list_flags))
				{
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Color32{ 59, 0, 135 }.to_uint());
					ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

					ImGui::SameLine();
					ImGui::Text(ent->get_type().classname);

					draw_components(ent);

					ImGui::EndTable();
				}
			}
		}


	}
	ImGui::End();
}

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
		}
		if (!metadata) {
			ImGui::Text("Asset has no metadata: %s\n", prop->range_hint);
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


			if (ImGui::GetIO().MouseDoubleClicked[0]) {
				if (metadata->tool_to_edit_me()) {
					std::string cmdstr = "start_ed ";
					cmdstr += '"';
					cmdstr += metadata->get_type_name();
					cmdstr += '"';
					cmdstr += " ";
					cmdstr += '"';
					cmdstr += asset_str.c_str();
					cmdstr += '"';
					Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmdstr.c_str());
				}
			} else if (ImGui::GetIO().MouseClicked[0]) {
				global_asset_browser.filter_all();
				global_asset_browser.unset_filter(1 << metadata->self_index);
			}
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
						auto classtype = ClassBase::find_class(prop->range_hint);
						*ptr_to_asset = GetAssets().find_sync(resource->filename, classtype,0).get();// loader->load_asset(resource->filename);
						
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

	if(ss->has_only_one_selected()) {
		auto entity = ss->get_only_one_selected();
		printf("adding to grid: %s\n", entity->get_type().classname);

		auto ti = &entity->get_type();
		while (ti) {
			if (ti->props) {
				grid.add_property_list_to_grid(ti->props, entity.get());
			}
			ti = ti->super_typeinfo;
		}

		
		auto& comps = entity->get_all_components();

		if (selected_component == 0)
			selected_component = comps[0]->instance_id;
		if (eng->get_object(selected_component) == nullptr || eng->get_object(selected_component)->cast_to<EntityComponent>() == nullptr ||
			eng->get_object(selected_component)->cast_to<EntityComponent>()->get_owner() != entity.get())
			selected_component = comps[0]->instance_id;

		ASSERT(selected_component != 0);


		auto c = eng->get_object(selected_component)->cast_to<EntityComponent>();
		printf("adding to grid: %s\n", c->get_type().classname);

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
	ed_doc.on_entity_will_delete.add(this, &SelectionState::on_node_deleted);
	ed_doc.on_close.add(this, &SelectionState::on_close);
}


DECLARE_ENGINE_CMD(STRESS_TEST)
{
	static int counter = 0;

	auto model = GetAssets().find_sync<Model>("wall2x2.cmdl");
	for (int z = 0; z < 20; z++) {
		for (int y = 0; y < 20; y++) {
			for (int x = 0; x < 20; x++) {
				glm::vec3 p(x, y, z + counter * 20);
				glm::mat4 transform = glm::translate(glm::mat4(1), p*2.0f);

				auto ent = eng->get_level()->spawn_entity_class<StaticMeshEntity>();
				ent->Mesh->set_model(model.get());
				ent->set_ws_transform(transform);
				
			}
		}
	}
	counter++;
}
#include "Render/MaterialPublic.h"
#if 0
DECLARE_ENGINE_CMD(STRESS_TEST_DECAL)
{
	for (int z = 0; z < 10; z++) {
		for (int y = 0; y < 10; y++) {
			for (int x = 0; x < 10; x++) {
				glm::vec3 p(x, y, z);
				glm::mat4 transform = glm::translate(glm::mat4(1), p * 2.0f);

				auto ent = eng->spawn_entity_class<DecalEntity>();
				ent->Decal->set_material(GetAssets().find_sync<MaterialInstance>("bulletDecal").get());
				ent->set_ws_transform(transform);
			}
		}
	}
}
#endif


EditorDoc::EditorDoc() {
	gui = std::make_unique<EditorUILayout>();

	gui->key_down_delegate.add(this, &EditorDoc::on_key_down);
	gui->mouse_down_delegate.add(this, &EditorDoc::on_mouse_down);
	gui->mouse_drag_delegate.add(this, &EditorDoc::on_mouse_drag);

	gui->wheel_delegate.add(this, &EditorDoc::on_mouse_wheel);

	command_mgr = std::make_unique<UndoRedoSystem>();
	gui->key_down_delegate.add(command_mgr.get(), &UndoRedoSystem::on_key_event);

	selection_state = std::make_unique<SelectionState>();
	prop_editor = std::make_unique<EdPropertyGrid>();
	manipulate = std::make_unique<ManipulateTransformTool>();
	outliner = std::make_unique<ObjectOutliner>();

}