#include "EditorDocLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "Game_Engine.h"
#include <algorithm>
#include "DrawPublic.h"
#include "glm/gtx/euler_angles.hpp"
#include "Framework/MeshBuilder.h"
#include "Framework/Dict.h"
#include "Framework/Files.h"
EditorDoc ed_doc;
IEditorTool* g_editor_doc = &ed_doc;

class TransformCommand : public Command
{
public:
	TransformCommand(EditorDoc* doc, const std::shared_ptr<EditorNode>& n, TransformType type, glm::vec3 delta)
		: doc(doc), node(n), type(type),delta(delta) {}

	TransformCommand(EditorDoc* doc, const std::shared_ptr<EditorNode>& n, glm::quat delta_q)
		: doc(doc), node(n), type(ROTATION), delta_q(delta_q) {}

	~TransformCommand() {}
	void execute() {
		if (type == TRANSLATION)
			node->position += delta;
		else if (type == ROTATION)
			node->rotation += delta_q;
		else if (type == SCALE)
			node->scale += delta;
	}
	void undo() {
		if (type == TRANSLATION)
			node->position -= delta;
		else if (type == ROTATION)
			node->rotation -= delta_q;
		else if (type == SCALE)
			node->scale -= delta;
	}

	std::shared_ptr<EditorNode> node;
	glm::vec3 delta;
	glm::quat delta_q;
	TransformType type;
	EditorDoc* doc{};
};

class EditPropertyCommand : public Command
{
public:

	EditPropertyCommand(EditorDoc* doc, const std::shared_ptr<EditorNode>& n, std::string key, std::string changeval)
		: doc(doc), node(n), newval(changeval),key(key) {}
	~EditPropertyCommand() {}
	void execute() {

		Dict* d = &node->get_dict();

		oldval = d->get_string(key.c_str());
		d->set_string(key.c_str(), newval.c_str());
	}
	void undo() {

		Dict* d = &node->get_dict();

		d->set_string(key.c_str(), oldval.c_str());
	}

	std::shared_ptr<EditorNode> node;
	std::string oldval;
	std::string key;
	std::string newval;
	EditorDoc* doc;
};

class CreateNodeCommand : public Command
{
public:
	CreateNodeCommand(const std::shared_ptr<EditorNode>& node, EditorDoc* doc) 
		: doc(doc), node(node) {}
	~CreateNodeCommand() {
	}

	void execute() {
		doc->nodes.push_back(node);
		node->show();
	}
	void undo() {
		assert(doc->nodes.back().get() == node.get());
		node->hide();
		doc->nodes.pop_back();
	}

	Dict spawn_dict;
	std::shared_ptr<EditorNode> node;
	EditorDoc * doc;
};

class RemoveNodeCommand : public Command
{
public:
	RemoveNodeCommand(EditorDoc* doc, const std::shared_ptr<EditorNode>& n) 
		: doc(doc), node(n) {
		index = doc->get_node_index(n.get());
		assert(index != -1);
	}
	~RemoveNodeCommand() {
	}

	void execute() {
		node->hide();
		doc->nodes.erase(doc->nodes.begin() + index);
	}
	void undo() {
		doc->nodes.insert(doc->nodes.begin() + index,node);
		node->show();
	}

	std::shared_ptr<EditorNode> node;
	EditorDoc* doc;
	int index = 0;
};

static float alphadither = 0.0;
void menu_temp()
{
	ImGui::SliderFloat("alpha", &alphadither, 0.0, 1.0);
}


static bool has_extension(const std::string& path, const std::string& ext)
{
	auto find = path.rfind('.');
	if (find == std::string::npos)
		return false;
	return path.substr(find + 1) == ext;
}


void AssetBrowser::init()
{
	Debug_Interface::get()->add_hook("dither", menu_temp);

	edmodels.clear();

	const char*const  model_root = "./Data/Models";
	const int mr_len = strlen(model_root);
	FileTree tree = FileSys::find_files(model_root);
	for (const auto& file : tree) {
		if(has_extension(file, ".c_mdl")) {
			EdModel em{};
			em.name = file.substr(mr_len + 1);
			edmodels.push_back(em);
		}
	}

	update_remap();

	asset_name_filter[0] = 0;
}
void AssetBrowser::handle_input(const SDL_Event& inp)
{
	if (inp.type == SDL_MOUSEWHEEL)
		increment_index(inp.wheel.y);

	if (inp.type == SDL_MOUSEBUTTONUP && drawing_model && inp.button.button == 1) {
		// create a new model
		bool spawn_another = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LCTRL];

		Dict d;
		d.set_vec3("position", model_position);
		d.set_string("name", "new_mod");
		d.set_string("classname", "static_mesh");
		d.set_string("model", edmodels[selected_real_index].name.c_str());

		//EditorNode* node = doc->create_node_from_type("static_mesh");
		//node->get_dict().set_vec3("position", model_position);
		//node->get_dict().set_string("model", edmodels[selected_real_index].name.c_str());
		//node->update_from_dict();
		//
		//std::shared_ptr<EditorNode> shared_ptr_node(node);
		//
		//CreateNodeCommand* com = new CreateNodeCommand(shared_ptr_node,doc);
		//doc->command_mgr.add_command(com);
		//
		//if (!spawn_another) {
		//	close();
		//	doc->selected_node = shared_ptr_node;
		//	doc->mode = EditorDoc::TOOL_NONE;
		//}

	}

}
void AssetBrowser::open(bool keyfocus)
{
	set_keyboard_focus = keyfocus;
}

RayHit EditorDoc::cast_ray_into_world(Ray* out_ray)
{
	Ray r;
	r.pos = camera.position;

	int x, y;
	SDL_GetMouseState(&x, &y);
	int wx, wy;
	SDL_GetWindowSize(eng->window, &wx, &wy);
	glm::vec3 ndc = glm::vec3(float(x) / wx, float(y) / wy, 0);
	ndc = ndc * 2.f - 1.f;
	ndc.y *= -1;

	glm::mat4 invviewproj = glm::inverse(vs_setup.viewproj);
	glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
	point /= point.w;

	glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

	r.dir = dir;

	if (out_ray)
		*out_ray = r;

	return eng->phys.trace_ray(r, -1, PF_ALL);
}

Color32 to_color32(glm::vec4 v) {
	Color32 c;
	c.r = glm::clamp(v.r * 255.f,0.f,255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}


void AssetBrowser::update()
{
	if (get_model()) {
		if (!drawing_model) {
			temp_place_model = idraw->register_obj();
			temp_place_model2 = idraw->register_obj();
		}

		drawing_model = true;

		Ray r;
		RayHit rh = doc->cast_ray_into_world(&r);
		if (rh.dist > 0) {
			model_position = rh.pos;
		}
		else {
			model_position = r.pos + r.dir * 10.f;
		}

		Render_Object obj;
		obj.model = get_model();

		obj.transform = glm::translate(glm::mat4(1),model_position);
		obj.visible = true;
		obj.param1 = to_color32(glm::vec4(1.0, 0.5, 0, alphadither));
		obj.dither = true;
		//obj.color_overlay = true;
		idraw->update_obj(temp_place_model, obj);

		auto mod = mods.find_or_load("sphere.glb");
		obj.model = mod;
		obj.opposite_dither = true;
		//obj.visible = false;
		idraw->update_obj(temp_place_model2, obj);

	}
	else {
		if (drawing_model) {
			idraw->remove_obj(temp_place_model);
			idraw->remove_obj(temp_place_model2);
		}

		drawing_model = false;
	}
}

void AssetBrowser::close()
{
	drawing_model = false;
	if (temp_place_model.is_valid()) {
		idraw->remove_obj(temp_place_model);
		idraw->remove_obj(temp_place_model2);
	}
}



Dict& EditorNode::get_dict()
{
	return edit_ent;
}


EditorNode::~EditorNode()
{
	if (render_handle.is_valid())
		idraw->remove_obj(render_handle);
	if (render_light.is_valid())
		idraw->remove_light(render_light);
}

void EditorNode::show()
{
	idraw->remove_obj(render_handle);
	idraw->remove_light(render_light);
	if (model) {
		render_handle = idraw->register_obj();
	}
}
void EditorNode::hide()
{
	idraw->remove_obj(render_handle);
	idraw->remove_light(render_light);
}



glm::mat4 EditorNode::get_transform()
{
	glm::mat4 transform;
	transform = glm::translate(glm::mat4(1), position);
	transform = transform * glm::mat4_cast(rotation);
	transform = glm::scale(transform, scale);
	return transform;
}
void EditorNode::init_on_new_espawn()
{
	
}

EditorNode* EditorDoc::create_node_from_dict(const Dict& d)
{
	return nullptr;
}

void EditorDoc::on_change_focus(editor_focus_state newstate)
{
}

void EditorDoc::open(const char* levelname)
{
	nodes.clear();
	
	bool good = editing_map.parse(levelname);

	for (int i = 0; i < editing_map.spawners.size(); i++) {
		EditorNode* node = create_node_from_dict(editing_map.spawners[i].dict);
		if(node)
			nodes.push_back(std::shared_ptr<EditorNode>(node));
	}
	assets.init();
}

void EditorDoc::save_doc()
{
}

void EditorDoc::close()
{
	nodes.clear();
}

void EditorDoc::leave_transform_tool(bool apply_delta)
{
	mode = TOOL_NONE;
}

void EditorDoc::enter_transform_tool(TransformType type)
{
	if (selected_node == nullptr) return;
	transform_tool_type = type;
	axis_bit_mask = 7;
	transform_tool_origin = selected_node->position;
	mode = TOOL_TRANSFORM;
}

bool EditorDoc::handle_event(const SDL_Event& event)
{
	if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse)
		return false;


	switch (mode)
	{
	case TOOL_NONE:
		if (event.type == SDL_KEYDOWN) {
			uint32_t scancode = event.key.keysym.scancode;
			if (scancode == SDL_SCANCODE_M) {
				mode = TOOL_SPAWN_MODEL;
				assets.open(true);
			}
			else if (scancode == SDL_SCANCODE_G) {
				enter_transform_tool(TRANSLATION);
			}
			else if (scancode == SDL_SCANCODE_R) {
				enter_transform_tool(ROTATION);
			}
			else if (scancode == SDL_SCANCODE_S) {
				enter_transform_tool(SCALE);
			}
			else if (scancode == SDL_SCANCODE_BACKSPACE) {
				if (selected_node != nullptr) {
					RemoveNodeCommand* com = new RemoveNodeCommand(this, selected_node);
					command_mgr.add_command(com);
				}
			}
		}
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			if (event.button.button == 1) {
				RayHit rh = cast_ray_into_world(nullptr);
				if (rh.dist > 0 && rh.ent_id > 0) {
					selected_node = nodes[rh.ent_id];
				}
				else {
					selected_node = nullptr;
				}
			}
		}
		break;
	case TOOL_SPAWN_MODEL:
		assets.handle_input(event);
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.scancode == SDL_SCANCODE_M) {
				assets.close();
				mode = TOOL_NONE;
			}
		}
		break;

	case TOOL_TRANSFORM:
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			if (event.button.button == 1)
				leave_transform_tool(true);
			if (event.button.button == 3)
				leave_transform_tool(false);
		}

		else if (event.type == SDL_KEYDOWN) {
			uint32_t scancode = event.key.keysym.scancode;
			bool has_shift = event.key.keysym.mod & KMOD_SHIFT;
			if (scancode == SDL_SCANCODE_X) {
				if (!has_shift)
					axis_bit_mask = (1 << 1) | (1 << 2);
				else
					axis_bit_mask = 1;
			}
			else if (scancode == SDL_SCANCODE_Y) {
				if (!has_shift)
					axis_bit_mask = (1) | (1 << 2);
				else
					axis_bit_mask = (1<<1);
			}
			else if (scancode == SDL_SCANCODE_Z) {
				if (!has_shift)
					axis_bit_mask = 1 | (1 << 1);
				else
					axis_bit_mask = (1<<2);
			}
			else if (scancode == SDL_SCANCODE_L) {
				local_transform = !local_transform;
			}
		}
		break;
	}

	// dont undo in middle of transforms
	if (mode != TOOL_TRANSFORM) {
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.scancode == SDL_SCANCODE_Z && event.key.keysym.mod & KMOD_CTRL)
				command_mgr.undo();
		}
	}
	if (eng->game_focused) {
		if (event.type == SDL_MOUSEWHEEL) {
			camera.scroll_callback(event.wheel.y);
		}
	}
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.scancode == SDL_SCANCODE_O)
			camera.orbit_mode = !camera.orbit_mode;
	}

	return true;
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

void EditorDoc::tick(float dt)
{

	{
		int x=0, y=0;
		if(eng->game_focused)
			SDL_GetRelativeMouseState(&x, &y);
		camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
	}

	auto window_sz = eng->get_game_viewport_dimensions();
	vs_setup = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);

	// build physics world

	eng->phys.ClearObjs();
	//{
	//	PhysicsObject obj;
	//	obj.is_level = true;
	//	obj.solid = true;
	//	obj.is_mesh = true;
	//	obj.mesh.structure = &eng->level->collision->bvh;
	//	obj.mesh.verticies = &eng->level->collision->verticies;
	//	obj.mesh.tris = &eng->level->collision->tris;
	//	obj.userindex = -1;
	//
	//	eng->phys.AddObj(obj);
	//}

	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->model) {
			PhysicsObject obj;
			obj.is_level = false;
			obj.solid = false;
			obj.is_mesh = false;

			Bounds b = transform_bounds(nodes[i]->get_transform(), nodes[i]->model->get_bounds());
			obj.min_or_origin = b.bmin;
			obj.max = b.bmax;
			obj.userindex = i;

			eng->phys.AddObj(obj);
		}
	}


	switch (mode)
	{
	case TOOL_SPAWN_MODEL:
		assets.update();
		break;

	case TOOL_TRANSFORM:
		transform_tool_update();


		break;
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

	if(good2)
		selected_node->position = intersect_point;

}


void EditorDoc::overlay_draw()
{
	static MeshBuilder mb;
	mb.Begin();
	mb.PushLineBox(glm::vec3(-1), glm::vec3(1), COLOR_BLUE);
	if (selected_node) {
		if (selected_node->model) {
			Model* m = selected_node->model;
			auto transform = selected_node->get_transform();
			Bounds b = transform_bounds(transform,m->get_bounds());
			mb.PushLineBox(b.bmin, b.bmax, COLOR_RED);

		}
	}
	mb.End();
	mb.Draw(GL_LINES);
}

uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}


void EditorDoc::draw_frame()
{
	auto vs = get_vs();
	idraw->scene_draw(vs, this);
}

void EditorDoc::imgui_draw()
{
	if (mode == TOOL_SPAWN_MODEL) assets.imgui_draw();

    static int item_selected = 0;

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	float alpha = (eng->game_focused) ? 0.3 : 0.7;

	ImGui::SetNextWindowBgAlpha(alpha);
	//ImGui::SetNextWindowSize(ImVec2(250, 500));
	uint32_t flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
	if (ImGui::Begin("EdDoc",nullptr, flags))
	{

		ImGui::DragFloat3("cam pos", &camera.position.x);
		ImGui::DragFloat("cam pitch", &camera.pitch);
		ImGui::DragFloat("cam yaw", &camera.yaw);


		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("Entity list",1, ent_list_flags, ImVec2(0, 300.f), 0.f))
        {
            // Declare columns
            // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
            // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();


            for (int row_n = 0; row_n < nodes.size(); row_n++)
            {
				EditorNode* n = nodes[row_n].get();

				const char* name = n->get_name();
                //if (!filter.PassFilter(item->Name))
                //    continue;

                const bool item_is_selected = item_selected == row_n;
                ImGui::PushID(row_n);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);

				ImU32 row_bg_color = color_to_uint(n->get_object_color());
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_bg_color);

                // For the demo purpose we can select among different type of items submitted in the first column
                ImGui::TableSetColumnIndex(0);
                char label[32];
                //sprintf(label, "%04d", item->ID);
                {
                    ImGuiSelectableFlags selectable_flags =ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    if (ImGui::Selectable(name, item_is_selected, selectable_flags, ImVec2(0, 0.f)))
                    {
						item_selected = row_n;
						selected_node = nodes[row_n];
                    }
                }

				ImGui::PopID();
            }
			ImGui::EndTable();
        }

		if (selected_node != nullptr) {

			ImGui::SeparatorText("Property editor");

			uint32_t prop_editor_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

			if (ImGui::BeginTable("Property editor", 2, prop_editor_flags))
			{
				ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed,80.f, 0);
				ImGui::TableSetupColumn("Value", 0, 0.0f, 1);
				ImGui::TableHeadersRow();

				ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

				Dict* d = &selected_node->get_dict();

				int cell = 0;
				for (auto& kv : d->keyvalues)
				{
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::PushID(cell);
					ImGui::TextUnformatted(kv.first.c_str());

					ImGui::TableSetColumnIndex(1);
					static char buffer[256];
					memcpy(buffer, kv.second.c_str(), kv.second.size());
					buffer[kv.second.size()] = 0;
					if (ImGui::InputText("##cell", buffer, 256)) {
						kv.second = buffer;
					}
					ImGui::PopID();

					cell++;
				}
				ImGui::PopStyleColor();
				ImGui::EndTable();
			}



			ImGui::DragFloat3("Position", &selected_node->position.x);
			ImGui::DragFloat3("Rotation", &selected_node->rotation.x);
			ImGui::DragFloat3("Scale", &selected_node->scale.x);
			


		}
	}
	ImGui::End();
}

Model* AssetBrowser::get_model()
{
	if (selected_real_index == -1) return nullptr;
	EdModel* em = &edmodels[selected_real_index];
	if (em->havent_loaded) {
		em->m = mods.find_or_load(em->name.c_str());
		em->havent_loaded = false;
	}
	return em->m;
}

void AssetBrowser::increment_index(int amt)
{
	if (remap.size() == 0) {
		return;
	}

	int remapped_index = -1;
	for (int i = 0; i < remap.size(); i++) {
		if (remap[i] == selected_real_index) {
			remapped_index = i;
			break;
		}
	}
	if (remapped_index == -1) {
		selected_real_index = remap[0];
	}
	else {
		remapped_index -= amt;
		remapped_index %= remap.size();
		if (remapped_index < 0) remapped_index += remap.size();
		selected_real_index = remap[remapped_index];
	}
}

void AssetBrowser::update_remap()
{
	remap.clear();
	for (int i = 0; i < edmodels.size(); i++) {
		if (filter_match_case) {
			if (edmodels[i].name.find(asset_name_filter) == std::string::npos) continue;
		}
		else {
			static std::string s;
			static std::string s2;
			s = edmodels[i].name;
			for (int i = 0; i < s.size(); i++)s[i] = toupper(s[i]);
			s2 = asset_name_filter;
			for (int j = 0; j < s2.size(); j++)s2[j] = toupper(s2[j]);
			if (s.find(s2) == std::string::npos) continue;
		}
		remap.push_back(i);
	}


	std::sort(remap.begin(), remap.end(),
		[&](int a, int b) {
			return edmodels[a].name < edmodels[b].name;
		});
}

void AssetBrowser::imgui_draw()
{
	if (ImGui::Begin("EdModels")) {

		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

		if (set_keyboard_focus) {
			ImGui::SetKeyboardFocusHere();
			set_keyboard_focus = false;
		}
		static bool match_case = false;
		if (ImGui::InputTextWithHint("Find", "search for model", asset_name_filter, 256)) {
			update_remap();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Match Case", &match_case);


		if (ImGui::BeginTable("Model list", 1, ent_list_flags, ImVec2(0, 300.f), 0.f))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();

			for (int row_n = 0; row_n < remap.size(); row_n++)
			{
				int this_real_index = remap[row_n];

				const char* name = edmodels[this_real_index].name.c_str();
				bool item_is_selected = this_real_index == selected_real_index;
				ImGui::PushID(row_n);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);
				ImGui::TableSetColumnIndex(0);
				{
					ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
					if (ImGui::Selectable(name, item_is_selected, selectable_flags, ImVec2(0, 0.f))) {
						selected_real_index = this_real_index;
					}
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

const View_Setup& EditorDoc::get_vs()
{
	return vs_setup;
}
