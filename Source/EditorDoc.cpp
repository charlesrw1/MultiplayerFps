#include "EditorDoc.h"
#include "imgui.h"
#include "glad/glad.h"
#include "Game_Engine.h"
#include <algorithm>
#include "Draw.h"
#include "glm/gtx/euler_angles.hpp"

class TransformCommand : public Command
{
	TransformCommand(EditorDoc* doc, int node_idx, TransformType type, glm::vec3 delta) 
		: doc(doc),node_idx(node_idx), type(type),delta(delta) {}
	~TransformCommand() {}
	void execute() {
		EditorNode* node = doc->nodes[node_idx].get();
		if (type == TRANSLATION)
			node->position += delta;
		else if (type == ROTATION)
			node->rotation += delta;
		else if (type == SCALE)
			node->scale += delta;

		node->save_out_to_level();
	}
	void undo() {
		EditorNode* node = doc->nodes[node_idx].get();
		if (type == TRANSLATION)
			node->position -= delta;
		else if (type == ROTATION)
			node->rotation -= delta;
		else if (type == SCALE)
			node->scale -= delta;

		node->save_out_to_level();
	}

	int node_idx;
	glm::vec3 delta;
	TransformType type;
	EditorDoc* doc;
};

class EditPropertyCommand : public Command
{
	enum Type {
		ADD_PROPERTY,
		REMOVE_PROPERTY,
		CHANGE_PROPERTY,
	};

	EditPropertyCommand(EditorDoc* doc, int node_idx, std::string key, Type type, std::string changeval)
		: doc(doc), node_idx(node_idx), type(type), newval(changeval) {}
	~EditPropertyCommand() {}
	void execute() {
		EditorNode* node = doc->nodes[node_idx].get();
		Dict* d = &node->entity_dict;
		if (type == ADD_PROPERTY) {
			d->set_string(key.c_str(), newval.c_str());
		}
		else if (type == REMOVE_PROPERTY) {
			oldval = d->get_string(key.c_str());
			d->remove_key(key.c_str());
		}
		else {
			oldval = d->get_string(key.c_str());
			d->set_string(key.c_str(), newval.c_str());
		}

		node->save_out_to_level();
	}
	void undo() {
		EditorNode* node = doc->nodes[node_idx].get();
		Dict* d = &node->entity_dict;
		if (type == ADD_PROPERTY) {
			d->remove_key(key.c_str());
		}
		else if (type == REMOVE_PROPERTY) {
			d->set_string(key.c_str(), oldval.c_str());
		}
		else {
			d->set_string(key.c_str(), oldval.c_str());
		}

		node->save_out_to_level();
	}

	int node_idx;
	Type type;
	std::string oldval;
	std::string key;
	std::string newval;
	EditorDoc* doc;
};

class CreateNodeCommand : public Command
{
	CreateNodeCommand(EditorDoc* doc, EditorNode* node) : doc(doc), node(node) {}
	~CreateNodeCommand() {
	}

	void execute() {
		doc->nodes.push_back(node);
		node->on_create();
	}
	void undo() {
		node->on_remove();
		doc->nodes.pop_back();
	}

	std::shared_ptr<EditorNode> node;
	EditorDoc * doc;
};

class RemoveNodeCommand : public Command
{
	RemoveNodeCommand(EditorDoc* doc, int nodeidx) : doc(doc), nodeidx(nodeidx) {}
	~RemoveNodeCommand() {
	}

	void execute() {
		node = doc->nodes[nodeidx];
		node->on_remove();
		doc->on_add_or_remove_node(node->dict_index, node->obj, node->_varying_obj_index, true);
		doc->nodes.erase(doc->nodes.begin() + nodeidx);
	}
	void undo() {
		doc->on_add_or_remove_node(node->dict_index, node->obj, node->_varying_obj_index, false);
		doc->nodes.insert(doc->nodes.begin() + nodeidx,node);
		node->on_create();
	}
	int nodeidx;
	std::shared_ptr<EditorNode> node;
	EditorDoc* doc;
};


void EditorNode::on_create()
{

}

void EditorNode::on_remove()
{
	// make sure any editor references are gone
	if (doc->selected_node == this)
		doc->selected_node = nullptr;

	// edit the actual engine stuff
	//Level::Entity_Spawn& es = doc->leveldoc->espawns.at(index);
}



void AssetBrowser::init()
{
	edmodels.clear();
	char buffer[256];
	while (Files::iterate_files_in_dir("./Data/Models/*", buffer, 256)) {
		EdModel em;
		em.name = buffer; // load the models once you need them
		em.m = nullptr;
		edmodels.push_back(em);
	}
	update_remap();

	asset_name_filter[0] = 0;
}
void AssetBrowser::handle_input(const SDL_Event& inp)
{
	if (inp.type == SDL_MOUSEWHEEL)
		increment_index(inp.wheel.y);
}
void AssetBrowser::open(bool keyfocus)
{
	set_keyboard_focus = keyfocus;
}

void AssetBrowser::update()
{
	if (get_model()) {
		drawing_model = true;

		Ray r;
		r.pos = doc->camera.position;
		
		int x, y;
		SDL_GetMouseState(&x, &y);
		int wx, wy;
		SDL_GetWindowSize(eng->window, &wx, &wy);
		glm::vec3 ndc = glm::vec3(float(x) / wx, float(y) / wy, 0);
		ndc = ndc * 2.f - 1.f;
		ndc.y *= -1;

		glm::mat4 invviewproj = glm::inverse(doc->vs_setup.viewproj);
		glm::vec4 point = invviewproj * vec4(ndc,1.0);
		point /= point.w;

		glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);
		
		r.dir = dir;

		RayHit rh = eng->phys.trace_ray(r, -1, PF_WORLD);
		if (rh.hit_world) {
			model_position = rh.pos;
		}
		else {
			model_position = r.pos + r.dir * 10.f;
		}
	}
	else {
		drawing_model = false;
	}
}

void AssetBrowser::close() {}

void EditorDoc::on_add_or_remove_node(int ent_dict_index, EdObjType type, int index, bool is_removal)
{
	for (int i = 0; i < nodes.size(); i++) {
		if (is_removal && nodes[i]->dict_index > ent_dict_index)
			nodes[i]->dict_index -= 1;
		if (!is_removal && nodes[i]->dict_index <= ent_dict_index)
			nodes[i]->dict_index += 1;

		if (nodes[i]->obj == type) {

			if (is_removal && nodes[i]->_varying_obj_index > index)
				nodes[i]->_varying_obj_index -= 1;
			else if (!is_removal && nodes[i]->_varying_obj_index <= index)
				nodes[i]->_varying_obj_index += 1;
		}
	}


}

extern Static_Mesh_Object make_static_mesh_from_dict(Level::Entity_Spawn* obj);
extern Level_Light make_light_from_dict(Level::Entity_Spawn* obj);

void EditorNode::save_out_to_level()
{
	Level::Entity_Spawn* espawn = &doc->leveldoc->espawns[dict_index];

	// save out
	entity_dict.set_vec3("position", position);
	entity_dict.set_vec3("rotation", rotation);
	entity_dict.set_vec3("scale", scale);
	espawn->spawnargs = entity_dict;
	espawn->name = entity_dict.get_string("name");
	espawn->classname = entity_dict.get_string("classname");
	espawn->position = position;
	espawn->position = rotation;
	espawn->position = scale;

	if (obj == EDOBJ_MODEL) {
		Static_Mesh_Object* obj = &doc->leveldoc->static_mesh_objs.at(_varying_obj_index);
		*obj = make_static_mesh_from_dict(espawn);
	}
	else if (obj == EDOBJ_LIGHT) {
		Level_Light* obj = &doc->leveldoc->lights.at(_varying_obj_index);
		*obj = make_light_from_dict(espawn);
	}
}

void EditorNode::make_from_existing(int existing_index)
{
	dict_index = existing_index;
	Level::Entity_Spawn* espawn = &doc->leveldoc->espawns[dict_index];

	position = espawn->position;
	rotation = espawn->rotation;
	scale = espawn->scale;

	// make a full copy of the dict
	entity_dict = espawn->spawnargs;
	_varying_obj_index = espawn->_ed_varying_index_for_statics;
	
	// in case these weren't set already
	entity_dict.set_string("name", espawn->name.c_str());
	entity_dict.set_string("classname", espawn->classname.c_str());

	if (espawn->classname == "light")
		obj = EDOBJ_LIGHT;
	else if (espawn->classname == "cubemap" || espawn->classname == "cubemap_box")
		obj = EDOBJ_CUBEMAPS;
	else if (espawn->classname == "static_mesh")
		obj = EDOBJ_MODEL;
	else
		obj = EDOBJ_GAMEOBJ;
}

void EditorDoc::open_doc(const char* levelname)
{
	leveldoc = eng->level;

	for (int i = 0; i < leveldoc->espawns.size(); i++) {
		Level::Entity_Spawn* espawn = &leveldoc->espawns[i];
		EditorNode* node = new EditorNode(this);
		node->make_from_existing(i);

		nodes.push_back(std::shared_ptr<EditorNode>(node));
	}
	assets.init();
}

void EditorDoc::save_doc()
{
}

void EditorDoc::close_doc()
{
}


bool EditorDoc::handle_event(const SDL_Event& event)
{
	if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse)
		return false;

	if (assets_open)
		assets.handle_input(event);

	switch (event.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		//SDL_SetRelativeMouseMode(SDL_TRUE);

		break;
	case SDL_MOUSEBUTTONUP:
		//SDL_SetRelativeMouseMode(SDL_FALSE);

		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.scancode)
		{
		case SDL_SCANCODE_M:
			if (assets_open)
				assets.close();
			else
				assets.open(true);
			assets_open = !assets_open;
			break;
		}


		break;
	}

	return true;
}

void EditorDoc::update()
{
	if (eng->game_focused) {
		int x, y;
		SDL_GetRelativeMouseState(&x, &y);
		camera.update_from_input(eng->keys, x, y);
	}
	View_Setup setup;
	setup.fov = glm::radians(70.f);
	setup.near = 0.01;
	setup.far = 100.0;
	setup.height = eng->window_h.integer();
	setup.width = eng->window_w.integer();
	setup.origin = camera.position;
	setup.front = camera.front;
	setup.view = camera.get_view_matrix();
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, setup.near, setup.far);
	setup.viewproj = setup.proj * setup.view;
	vs_setup = setup;


	// build physics world

	eng->phys.ClearObjs();
	{
		PhysicsObject obj;
		obj.is_level = true;
		obj.solid = true;
		obj.is_mesh = true;
		obj.mesh.structure = &eng->level->collision.bvh;
		obj.mesh.verticies = &eng->level->collision.verticies;
		obj.mesh.tris = &eng->level->collision.tris;

		eng->phys.AddObj(obj);
	}

	if (assets_open)
		assets.update();
	
}

void EditorDoc::scene_draw_callback()
{
	if (assets_open && assets.drawing_model) {
		Draw_Model_Frontend_Params p;
		p.model = assets.get_model();
		p.transform = glm::translate(glm::mat4(1), assets.model_position);
		p.wireframe_render = false;
		p.solidcolor_render = true;
		p.render_additive = true;
		p.colorparam = glm::vec4(1.0, 0.5, 0, (sin(GetTime()) * 0.5 + 0.5));
		draw.draw_model_immediate(p);
	}
}

void EditorDoc::overlays_draw()
{
}

uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}

uint32_t get_bg_color_for_ent(Level* l, EditorNode* node)
{
	const char* classname = node->entity_dict.get_string("classname");

	if (node->obj == EDOBJ_MODEL) {
		return color_to_uint({ 0, 140, 255 , 50 });
	}
	if (node->obj == EDOBJ_LIGHT) {
		return color_to_uint({ 255, 242, 0, 50 });
	}
	if (node->obj == EDOBJ_CUBEMAPS) {
		return color_to_uint({ 77, 239, 247, 50 });
	}
	return color_to_uint({ 50,50,50,50 });
}

void EditorDoc::imgui_draw()
{
	if (assets_open) assets.imgui_draw();

    static int item_selected = 0;

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	float alpha = (eng->game_focused) ? 0.3 : 0.7;

	ImGui::SetNextWindowBgAlpha(alpha);
	//ImGui::SetNextWindowSize(ImVec2(250, 500));
	uint32_t flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
	if (ImGui::Begin("EdDoc",nullptr, flags))
	{
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

				ImU32 row_bg_color = get_bg_color_for_ent(leveldoc, n);
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
						selected_node = n;
                    }
                }

				ImGui::PopID();
            }
        }
        ImGui::EndTable();

		if (selected_node != nullptr) {

			ImGui::SeparatorText("Property editor");

			uint32_t prop_editor_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

			if (ImGui::BeginTable("Property editor", 2, prop_editor_flags))
			{
				ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed,80.f, 0);
				ImGui::TableSetupColumn("Value", 0, 0.0f, 1);
				ImGui::TableHeadersRow();

				ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

				Dict* d = &selected_node->entity_dict;

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
			}
			ImGui::EndTable();



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
		em->m = FindOrLoadModel(em->name.c_str());
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
