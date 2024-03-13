#include "EditorDoc.h"
#include "imgui.h"
#include "glad/glad.h"
#include "Game_Engine.h"
#include <algorithm>
#include "Draw.h"
#include "glm/gtx/euler_angles.hpp"

class TransformCommand : public Command
{
public:
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
public:
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
		Dict* d = &node->get_dict();
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
		Dict* d = &node->get_dict();
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
public:
	CreateNodeCommand(EditorDoc* doc, EditorNode* node, Dict spawn_dict) : doc(doc), node(node), spawn_dict(spawn_dict) {}
	~CreateNodeCommand() {
	}

	void execute() {
		doc->nodes.push_back(node);
		Dict copy = spawn_dict;	// because on create is destructive...
		node->on_create_from_dict(-1,-1, &copy);
	}
	void undo() {
		node->on_remove();
		doc->nodes.pop_back();
	}

	Dict spawn_dict;
	std::shared_ptr<EditorNode> node;
	EditorDoc * doc;
};

class RemoveNodeCommand : public Command
{
public:
	RemoveNodeCommand(EditorDoc* doc, int nodeidx) : doc(doc), nodeidx(nodeidx) {}
	~RemoveNodeCommand() {
	}

	void execute() {
		node = doc->nodes[nodeidx];
		Level::Entity_Spawn& espawn = doc->leveldoc->espawns[node->dict_index];
		// make a copy of the underlying data, dont want to lose it
		obj_dict = std::move(espawn.spawnargs);
		obj_dict_index = node->dict_index;
		varying_index = node->_varying_obj_index;

		node->on_remove();	// removes the node from engine lists
		// decrements other node indicies to patch up removal
		doc->on_add_or_remove_node(node->dict_index, node->obj, node->_varying_obj_index, true);
		// removes node from editor list
		doc->nodes.erase(doc->nodes.begin() + nodeidx);
	}
	void undo() {
		doc->nodes.insert(doc->nodes.begin() + nodeidx,node);
		doc->on_add_or_remove_node(node->dict_index, node->obj, node->_varying_obj_index, false);
		node->on_create_from_dict(obj_dict_index, varying_index, &obj_dict);
	}
	int nodeidx;
	std::shared_ptr<EditorNode> node;
	EditorDoc* doc;

	int obj_dict_index = 0;
	int varying_index = 0;
	Dict obj_dict;
};


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

	if (inp.type == SDL_MOUSEBUTTONUP && drawing_model && inp.button.button == 1) {
		// create a new model
		bool spawn_another = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LCTRL];

		Dict d;
		d.set_vec3("position", model_position);
		d.set_string("name", "new_mod");
		d.set_string("classname", "static_mesh");
		d.set_string("model", edmodels[selected_real_index].name.c_str());

		EditorNode* node = new EditorNode(doc);
		CreateNodeCommand* com = new CreateNodeCommand(doc, node, std::move(d));
		doc->command_mgr.add_command(com);

		if (!spawn_another) {
			doc->selected_node = node;
			doc->mode = EditorDoc::TOOL_NONE;
		}

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
	glm::vec4 point = invviewproj * vec4(ndc, 1.0);
	point /= point.w;

	glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

	r.dir = dir;

	if (out_ray)
		*out_ray = r;

	return eng->phys.trace_ray(r, -1, PF_ALL);
}

void AssetBrowser::update()
{
	if (get_model()) {
		drawing_model = true;

		Ray r;
		RayHit rh = doc->cast_ray_into_world(&r);
		if (rh.dist > 0) {
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
		if (!is_removal && nodes[i]->dict_index > ent_dict_index)
			nodes[i]->dict_index += 1;

		if (nodes[i]->obj == type) {

			if (is_removal && nodes[i]->_varying_obj_index > index)
				nodes[i]->_varying_obj_index -= 1;
			else if (!is_removal && nodes[i]->_varying_obj_index > index)
				nodes[i]->_varying_obj_index += 1;
		}
	}


}

extern Static_Mesh_Object make_static_mesh_from_dict(Level::Entity_Spawn* obj);
extern Level_Light make_light_from_dict(Level::Entity_Spawn* obj);


Dict& EditorNode::get_dict()
{
	return doc->leveldoc->espawns[dict_index].spawnargs;
}

void EditorNode::on_create_from_dict(int ent_spawn_index, int varying_index, Dict* d)
{
	Level* level = doc->leveldoc;
	if (ent_spawn_index == -1) {
		ent_spawn_index = level->espawns.size();
	}
	level->espawns.insert(level->espawns.begin() + ent_spawn_index, Level::Entity_Spawn());
	doc->on_add_or_remove_node(ent_spawn_index, obj, varying_index, false);
	Level::Entity_Spawn& es = level->espawns.at(ent_spawn_index);
	dict_index = ent_spawn_index;

	es.spawnargs = std::move(*d);
	es.position = es.spawnargs.get_vec3("position");
	es.rotation = es.spawnargs.get_vec3("rotation");
	es.scale = es.spawnargs.get_vec3("scale", vec3(1.f));
	es.classname = es.spawnargs.get_string("classname");
	es.name = es.spawnargs.get_string("name", "emptyname");

	init_on_new_espawn();

	if (obj == EDOBJ_LIGHT) {
		if (varying_index == -1) {
			varying_index = level->lights.size();
		}
		level->lights.insert(level->lights.begin() + varying_index, make_light_from_dict(&es));
	}
	else if (obj == EDOBJ_MODEL) {
		if (varying_index == -1) {
			varying_index = level->static_mesh_objs.size();
		}
		level->static_mesh_objs.insert(level->static_mesh_objs.begin() + varying_index, make_static_mesh_from_dict(&es));
	}
	_varying_obj_index = varying_index;
	es._ed_varying_index_for_statics = _varying_obj_index;
}

void EditorNode::on_remove()
{
	// make sure any editor references are gone
	if (doc->selected_node == this)
		doc->selected_node = nullptr;

	// edit the actual engine stuff
	Level* level = doc->leveldoc;
	doc->leveldoc->espawns.erase(doc->leveldoc->espawns.begin() + dict_index);	// remove the entityspawn

	// remove specific stuff
	if (obj == EDOBJ_LIGHT) {
		level->lights.erase(level->lights.begin() + _varying_obj_index);
	}
	else if (obj == EDOBJ_MODEL) {
		level->static_mesh_objs.erase(level->static_mesh_objs.begin() + _varying_obj_index);
	}
}


void EditorNode::save_out_to_level()
{
	Level::Entity_Spawn* espawn = &doc->leveldoc->espawns[dict_index];

	// save out
	get_dict().set_vec3("position", position);
	get_dict().set_vec3("rotation", rotation);
	get_dict().set_vec3("scale", scale);
	espawn->name = get_dict().get_string("name");
	espawn->classname = get_dict().get_string("classname");
	espawn->position = position;
	espawn->rotation = rotation;
	espawn->scale = scale;

	if (obj == EDOBJ_MODEL) {
		Static_Mesh_Object* obj = &doc->leveldoc->static_mesh_objs.at(_varying_obj_index);
		*obj = make_static_mesh_from_dict(espawn);
	}
	else if (obj == EDOBJ_LIGHT) {
		Level_Light* obj = &doc->leveldoc->lights.at(_varying_obj_index);
		*obj = make_light_from_dict(espawn);
	}
}
glm::mat4 EditorNode::get_transform()
{
	glm::mat4 transform;
	transform = glm::translate(glm::mat4(1), position);
	transform = transform * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
	transform = glm::scale(transform, scale);
	return transform;
}
void EditorNode::init_on_new_espawn()
{
	const char* classname = get_dict().get_string("classname");
	if (strcmp(classname, "light")==0)
		obj = EDOBJ_LIGHT;
	else if (strcmp("cubemap", classname) == 0 || strcmp("cubemap_box", classname) == 0) {
		obj = EDOBJ_CUBEMAPS;
	}
	else if (strcmp("static_mesh", classname) == 0)
		obj = EDOBJ_MODEL;
	else
		obj = EDOBJ_GAMEOBJ;

	position = get_dict().get_vec3("position");
	rotation = get_dict().get_vec3("rotation");
	scale = get_dict().get_vec3("scale", vec3(1.f));

	const char* mod_name = get_dict().get_string("model");
	if (*mod_name) {
		model = FindOrLoadModel(mod_name);
	}
}

void EditorNode::make_from_existing(int existing_index)
{
	dict_index = existing_index;
	Level::Entity_Spawn* espawn = &doc->leveldoc->espawns[dict_index];
	_varying_obj_index = espawn->_ed_varying_index_for_statics;
	// in case these weren't set already
	get_dict().set_string("name", espawn->name.c_str());
	get_dict().set_string("classname", espawn->classname.c_str());
	get_dict().set_vec3("position", espawn->position);
	get_dict().set_vec3("rotation", espawn->rotation);
	get_dict().set_vec3("scale", espawn->scale);
	
	init_on_new_espawn();
}

void EditorDoc::open_doc(const char* levelname)
{
	nodes.clear();
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
					RemoveNodeCommand* com = new RemoveNodeCommand(this, get_node_index(selected_node));
					command_mgr.add_command(com);
				}
			}
		}
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			if (event.button.button == 1) {
				RayHit rh = cast_ray_into_world(nullptr);
				if (rh.dist > 0 && rh.ent_id > 0) {
					selected_node = nodes[rh.ent_id].get();
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
			if (event.key.keysym.scancode == SDL_SCANCODE_M)
				mode = TOOL_NONE;
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

void EditorDoc::update()
{
	View_Setup setup;
	setup.fov = glm::radians(70.f);
	setup.near = 0.01;
	setup.far = 100.0;
	setup.height = eng->window_h.integer();
	setup.width = eng->window_w.integer();
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, setup.near, setup.far);
	{
		int x=0, y=0;
		if(eng->game_focused)
			SDL_GetRelativeMouseState(&x, &y);
		camera.update_from_input(eng->keys, x, y, glm::inverse(setup.proj));
	}

	setup.origin = camera.position;
	setup.front = camera.front;
	setup.view = camera.get_view_matrix();
	setup.viewproj = setup.proj * setup.view;
	vs_setup = setup;

	// build physics world

	eng->phys.ClearObjs();
	{
		PhysicsObject obj;
		obj.is_level = true;
		obj.solid = true;
		obj.is_mesh = true;
		obj.mesh.structure = &eng->level->collision->bvh;
		obj.mesh.verticies = &eng->level->collision->verticies;
		obj.mesh.tris = &eng->level->collision->tris;
		obj.userindex = -1;

		eng->phys.AddObj(obj);
	}

	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->model) {
			PhysicsObject obj;
			obj.is_level = false;
			obj.solid = false;
			obj.is_mesh = false;

			Bounds b = transform_bounds(nodes[i]->get_transform(), nodes[i]->model->mesh.aabb);
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
	selected_node->save_out_to_level();

}

void EditorDoc::scene_draw_callback()
{
	switch (mode)
	{
	case TOOL_SPAWN_MODEL:
		if (assets.drawing_model) {
			Draw_Model_Frontend_Params p;
			p.model = assets.get_model();
			p.transform = glm::translate(glm::mat4(1), assets.model_position);
			p.wireframe_render = false;
			p.solidcolor_render = true;
			p.render_additive = true;
			p.colorparam = glm::vec4(1.0, 0.5, 0, (sin(GetTime()) * 0.5 + 0.5));
			draw.draw_model_immediate(p);
		}
		break;

	case TOOL_TRANSFORM:
		break;
	}

}


void EditorDoc::overlays_draw()
{
	static MeshBuilder mb;
	mb.Begin();
	mb.PushLineBox(glm::vec3(-1), glm::vec3(1), COLOR_BLUE);
	if (selected_node) {
		if (selected_node->model) {
			Model* m = selected_node->model;
			auto transform = selected_node->get_transform();
			Bounds b = transform_bounds(transform, m->mesh.aabb);
			mb.PushLineBox(b.bmin, b.bmax, COLOR_RED);



		}
	}
	mb.End();
	mb.Draw(GL_LINES);
}

uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}

uint32_t get_bg_color_for_ent(Level* l, EditorNode* node)
{
	const char* classname = node->get_dict().get_string("classname");

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
