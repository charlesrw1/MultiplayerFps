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
#include "Framework/MyImguiLib.h"
#include "AssetCompile/Someutils.h"
#include "Physics/Physics2.h"
#include <stdexcept>
EditorDoc ed_doc;
IEditorTool* g_editor_doc = &ed_doc;



static std::string to_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}


// hint string : "<default>,<min>,<max>"
// or: "<default>
// or: "<default>,<option1>,<option2>,.."

std::vector<StringView> parse_hint_string(const char* cstr)
{
	std::vector<StringView> out;
	int start = 0;
	size_t find = 0;
	std::string hint = cstr;
	while ((find = hint.find(',',start))!=std::string::npos) {
		out.push_back(StringView(cstr +start, (find-start)));
		start = find + 1;
	}
	out.push_back(StringView(cstr + start, (hint.size() - start)));
	return out;
}

bool EditorSchemaManager::load(const char* path)
{
	all_schema_list.clear();

	auto file = FileSys::open_read_os(path);
	if (!file)
		throw std::runtime_error("couldnt open file");

	DictParser in;
	in.load_from_file(file.get());
	StringView tok;
	while (in.read_string(tok) && !in.is_eof()) {
		auto os = ObjectSchema();
		os.name = to_string(tok);

		if (find_schema(os.name) != nullptr)
			throw std::runtime_error("schema has duplicate names " + os.name);


		in.read_string(tok);
		if (tok.cmp("abstract"))
			os.display_in_editor = false;
		else
			in.return_string(tok);

		in.read_string(tok);
		if (tok.cmp(":")) {
			in.read_string(tok);
			os.tooltip = to_string(tok);
		}
		else
			in.return_string(tok);


		if (!in.expect_item_start())
			throw std::runtime_error("expected item_start for " + os.name);

		while (in.read_string(tok) && !in.is_eof() && !in.check_item_end(tok)) {
			SchemaProperty prop;
			prop.type = to_string(tok);

			if (prop.type == "SIGNAL" || prop.type == "EVENT") {
				const bool is_signal = prop.type == "SIGNAL";
				in.read_string(tok);
				std::string name = to_string(tok);
				in.read_string(tok);
				std::string tooltip;
				if (tok.cmp(":")) {
					in.read_string(tok);
					tooltip = to_string(tok);
				}
				else
					in.return_string(tok);
				string_and_tooltip st;
				st.str = name;
				st.tooltip = tooltip;
				if (is_signal)
					os.connections.signals.push_back(st);
				else
					os.connections.events.push_back(st);

				continue;
			}
			else if (tok.cmp("_edimage")) {
				in.read_string(tok);
				os.edimage = to_string(tok);
				continue;
			}
			else if (tok.cmp("_edmodel")) {
				in.read_string(tok);
				os.edmodel = to_string(tok);
				continue;
			}
			else if (tok.cmp("_edcolor")) {
				float r, g, b;
				in.read_float3(r, g, b);
				os.edcolor.r = r;
				os.edcolor.g = g;
				os.edcolor.b = b;
				continue;
			}
			else if (tok.cmp("SET")) {
				in.read_string(tok);
				std::string name = to_string(tok);
				auto find = os.find_property_name(name);
				if (!find)
					throw std::runtime_error("SET called but couldn't find field to override " + os.name + " " + name);
				in.read_string(tok);
				std::string default_new = to_string(tok);

				// overwrite default
				auto vec = parse_hint_string(find->hint.c_str());
				for (int i = 1; i < vec.size(); i++) {
					default_new += ',';
					default_new += to_string(vec.at(i));
				}
				find->hint = default_new;
				continue;
			}


			in.read_string(tok);
			prop.name = to_string(tok);

			if (prop.name.size() > 0 && prop.name[0] == '_')
				prop.dont_expose = true;

			if (prop.type == "inherits") {
				auto find = find_schema(prop.name);
				if (!find)
					throw std::runtime_error("couldnt find 'inherits' type " + prop.name);

				os.inherit_from(find);

				continue;
			}

			prop.type = "Leveled_" + prop.type;

			in.read_string(tok);
			prop.hint = to_string(tok);

			in.read_string(tok);
			if (tok.cmp(":")) {
				in.read_string(tok);
				prop.tooltip = to_string(tok);
			}
			else
				in.return_string(tok);

			os.properties.push_back(prop);
		}

		all_schema_list.insert({ os.name,os });
	}

	return true;
}

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
	std::string to_string() override {
		return "EditProperty: " + std::string(node->get_schema_name()) + key;
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
	CreateNodeCommand(const std::shared_ptr<EditorNode>& node) :
		 node(node) {}
	~CreateNodeCommand() {
	}

	void execute() {
		ed_doc.nodes.push_back(node);
		node->show();
	}
	void undo() {
		assert(ed_doc.nodes.back().get() == node.get());
		node->hide();
		ed_doc.nodes.pop_back();

		ed_doc.remove_any_references(node.get());
	}
	std::string to_string() override {
		return "CreateNode: " + std::string(node->get_schema_name());
	}

	std::shared_ptr<EditorNode> node;
};



class RemoveNodeCommand : public Command
{
public:
	RemoveNodeCommand(const std::shared_ptr<EditorNode>& n) 
		:  node(n) {
		index = ed_doc.get_node_index(n.get());
		assert(index != -1);
	}
	~RemoveNodeCommand() {
	}

	void execute() {
		node->hide();
		ed_doc.nodes.erase(ed_doc.nodes.begin() + index);
		ed_doc.remove_any_references(node.get());
	}
	void undo() {
		ed_doc.nodes.insert(ed_doc.nodes.begin() + index,node);
		node->show();
	}
	std::string to_string() override {
		return "RemoveNode: " + std::string(node->get_schema_name());
	}
	
	std::shared_ptr<EditorNode> node;
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

// check every 5 seconds
ConfigVar g_assetbrowser_reindex_time("g_assetbrowser_reindex_time", "5.0", CVAR_FLOAT | CVAR_UNBOUNDED);

#include "Texture.h"


class EdResourceSchema : public EdResourceBase
{
public:
	EdResourceSchema(const std::string& path) {
		name = path;
	}
	const char* get_type_name() const override {
		return "Schema";
	}
	EdResType get_type() const override {
		return EdResType::Schema;
	}
	Color32 get_asset_color() const override { return { 252, 104, 93 }; }
};
class EdResourceModel : public EdResourceBase
{
public:
	EdResourceModel(const std::string& path) {
		name = path;
	}
	const char* get_type_name() const override {
		return "Model";
	}
	EdResType get_type() const override {
		return EdResType::Model;
	}
	Color32 get_asset_color() const override { return { 5, 168, 255 }; }
};

class EdResourceMaterial : public EdResourceBase
{
public:
	EdResourceMaterial(const std::string& path) {
		name = path;
	}
	const char* get_type_name() const override {
		return "Material";
	}
	EdResType get_type() const override {
		return EdResType::Material;
	}
};

EdResourceBase* AssetBrowser::search_for_file(const std::string& path)
{
	auto find = path_to_resource.find(path);
	return find == path_to_resource.end() ? nullptr : find->second;
}

void AssetBrowser::init()
{
	all_resources.clear();
	path_to_resource.clear();

	// loop through various directories and find files in them

	// MODELS
	auto find_tree = FileSys::find_files("./Data/Models");
	for (const auto file : find_tree) {
		if (has_extension(file, "cmdl")) {
			auto resource_exists = search_for_file(file);
			if (!resource_exists) {
				append_new_resource(new EdResourceModel(file));
			}
		}
		// if a .cmdl hasn't been compilied yet, still include .defs as .cmdls as they will be autocompilied
		else if (has_extension(file, "def")) {
			std::string path = strip_extension(file) + ".cmdl";
			auto resource_exists = search_for_file(path);
			if (!resource_exists) {
				append_new_resource(new EdResourceModel(path));
			}
		}
	}
	// MATERIALS
	// should proablly handle differently, all materials  gets indexed by material system at init time
	for (const auto& mat : mats.materials) {
		append_new_resource(new EdResourceMaterial(mat.first));
	}
	// SCHEMAS
	for (const auto& schema : ed_doc.ed_schema.all_schema_list) {
		if(schema.second.display_in_editor)
			append_new_resource(new EdResourceSchema(schema.first));
	}


	asset_name_filter[0] = 0;
}


// Unproject mouse coords into a vector, cast that into the world via physics
RayHit EditorDoc::cast_ray_into_world(Ray* out_ray, const int mx, const int my)
{
	Ray r;
	r.pos = camera.position;

	// get ui size
	const Rect2d size = get_size();
	const int wx = size.w;
	const int wy = size.h;
	glm::vec3 ndc = glm::vec3(float(mx - size.x) / wx, float(my - size.y) / wy, 0);
	ndc = ndc * 2.f - 1.f;
	ndc.y *= -1;

	glm::mat4 invviewproj = glm::inverse(vs_setup.viewproj);
	glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
	point /= point.w;

	glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

	r.dir = dir;

	if (out_ray)
		*out_ray = r;

	world_query_result res;
	g_physics->trace_ray(res, r.pos, r.dir, 100.0, {});

	Debug::add_line(r.pos, r.dir * 50.0f, COLOR_PINK, 10.0);

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
	
}


Dict& EditorNode::get_dict()
{
	return dictionary;
}


EditorNode::~EditorNode()
{
	hide();	// remove any handles
}

static Material* generate_spritemat_from_texture(Texture* t)
{
	// generate hash based on name, yes this is hacked
	StringName name_(t->name.c_str());
	Material* mat = mats.find_for_name(std::to_string(name_.get_hash()).c_str());
	if (mat)
		return mat;

	mat = mats.create_temp_shader(std::to_string(name_.get_hash()).c_str());
	mat->billboard = billboard_setting::FACE_CAMERA;
	mat->images[0] = t;
	mat->type = material_type::UNLIT;
	mat->alpha_tested = true;

	mat->diffuse_tint = glm::vec4(1.0);

	return mat;
}

Material* EditorNode::get_sprite_material()
{
	if (template_class && !template_class->edimage.empty()) {
		Texture* t = mats.find_texture(template_class->edimage.c_str());
		if (t) {
			return generate_spritemat_from_texture(t);
		}
	}
	Texture* t = mats.find_texture("icon/mesh.png");
	assert(t);
	return generate_spritemat_from_texture(t);

}
 void EditorNode::init_from_schema(const ObjectSchema* t) {
	template_class = t;
	dictionary = {};	// empty dict means deafult values

	for (int i = 0; i < template_class->properties.size(); i++) {
		auto& prop = template_class->properties[i];
		auto parsevec = parse_hint_string(prop.hint.c_str());
		if (parsevec.empty()) continue;
		dictionary.set_string(prop.name.c_str(), to_std_string_sv(parsevec[0]).c_str());
	}
}
void EditorNode::show()
{
	hide();

	Model* m = get_rendering_model();
	render_handle = idraw->register_obj();
	physics = g_physics->allocate_physics_actor();
	if (m) {
		Render_Object ro;
		ro.model = m;
		ro.transform = get_transform();
		ro.visible = true;
		ro.param1 = get_rendering_color();
		idraw->update_obj(render_handle, ro);

		glm::quat q;
		glm::vec3 p, s;
		read_transform_from_dict(p, q, s);
		glm::vec3 halfsize = (m->get_bounds().bmax - m->get_bounds().bmin)*0.5f*glm::abs(s);
		glm::vec3 center = m->get_bounds().get_center()*s;
		center = glm::mat4_cast(q) * glm::vec4(center, 1.0);
		PhysTransform tr;
		tr.position = p;
		tr.rotation = q;
		if (m->get_physics_body())
			physics->create_static_actor_from_model(m, tr);
		else
			physics->create_static_actor_from_shape(physics_shape_def::create_box(halfsize,p+ center,q));
	}
	else {
		Material* mat = get_sprite_material();
		Render_Object ro;
		ro.model = mods.get_sprite_model();

		glm::vec3 position = get_dict().get_vec3("position");
		ro.param1 = get_rendering_color();
		ro.transform = glm::scale(glm::translate(glm::mat4(1), position),glm::vec3(0.5));
		ro.mat_override = mat;
		ro.visible = true;
		idraw->update_obj(render_handle, ro);


		physics->create_static_actor_from_shape(physics_shape_def::create_sphere(position, 0.25));
	}
}
void EditorNode::hide()
{
	idraw->remove_obj(render_handle);
	g_physics->free_physics_actor(physics);
}


glm::mat4 EditorNode::get_transform()
{
	glm::vec3 position, scale;
	glm::quat rotation;
	read_transform_from_dict(position, rotation, scale);

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

EditorNode* EditorDoc::spawn_from_schema_type(const char* schema_type)
{
	auto template_obj = ed_schema.find_schema(schema_type);
	EditorNode* node = new EditorNode();
	node->set_uid(id_start);
	node->init_from_schema(template_obj);
	
	std::shared_ptr<EditorNode> shared(node);

	command_mgr.add_command(new CreateNodeCommand(shared) );	// add command

	return node;
}

void EditorDoc::on_change_focus(editor_focus_state newstate)
{
	if (newstate == editor_focus_state::Background) {

		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini leveldock.ini");
	}
	else if (newstate == editor_focus_state::Closed) {
		close();

		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini leveldock.ini");
	}
	else {
		// focused, stuff can start being rendered

		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  leveldock.ini");
	}
}

void EditorDoc::init()
{
	// set my parent
	set_parent(eng->get_gui());
}

void EditorDoc::open(const char* levelname)
{
	init();

	ed_schema.load("./Data/classes.txt");
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
	transform_tool_origin = selected_node->get_dict().get_vec3("position");
	mode = TOOL_TRANSFORM;
}

void EditorDoc::ui_paint() 
{
	size = parent->get_size();	// update my size
}

bool EditorDoc::handle_event(const SDL_Event& event)
{
	switch (mode)
	{
	case TOOL_NONE:
		if (event.type == SDL_KEYDOWN) {
			uint32_t scancode = event.key.keysym.scancode;
			if (scancode == SDL_SCANCODE_M) {
				//mode = TOOL_SPAWN_MODEL;
				//assets.open(true);
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
					RemoveNodeCommand* com = new RemoveNodeCommand(selected_node);
					command_mgr.add_command(com);
				}
			}
			else if (scancode == SDL_SCANCODE_N) {
				spawn_from_schema_type("NPC");
			}
		}
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			if (!get_size().is_point_inside(event.button.x, event.button.y))
				return false;

			if (event.button.button == 1) {
				RayHit rh = cast_ray_into_world(nullptr, event.button.x, event.button.y);
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
		//assets.handle_input(event);
		//if (event.type == SDL_KEYDOWN) {
		//	if (event.key.keysym.scancode == SDL_SCANCODE_M) {
		//		assets.close();
		//		mode = TOOL_NONE;
		//	}
		//}
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
	if (eng->get_game_focused()) {
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
		if (eng->get_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
		}
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

	//for (int i = 0; i < nodes.size(); i++) {
	//	if (nodes[i]->model) {
	//		PhysicsObject obj;
	//		obj.is_level = false;
	//		obj.solid = false;
	//		obj.is_mesh = false;
	//
	//		Bounds b = transform_bounds(nodes[i]->get_transform(), nodes[i]->model->get_bounds());
	//		obj.min_or_origin = b.bmin;
	//		obj.max = b.bmax;
	//		obj.userindex = i;
	//
	//		eng->phys.AddObj(obj);
	//	}
	//}


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
	if (selected_node) {
		Model* m = selected_node->get_rendering_model();
		if (m) {
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

void EditorDoc::remove_any_references(EditorNode* node)
{
	if (outliner.get_selected() == node)
		outliner.set_selected(nullptr);
	if (prop_editor.get_node() == node)
		prop_editor.set(nullptr);
}

void EditorDoc::imgui_draw()
{
	//if (mode == TOOL_SPAWN_MODEL) 
		assets.imgui_draw();

	outliner.draw();
	prop_editor.set(outliner.get_selected());
	prop_editor.draw();

	if (outliner.get_selected() != nullptr)
		outliner.get_selected()->show();	// update it

	return;
#if 0
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
#endif
}

void EditorDoc::hook_scene_viewport_draw()
{
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
		{
			EdResourceBase* resource = *(EdResourceBase**)payload->Data;
	
			EdResourceSchema* schema = dynamic_cast<EdResourceSchema*>(resource);
			if (schema) {
				spawn_from_schema_type(schema->get_asset_name().c_str());
			}
		}
		ImGui::EndDragDropTarget();
	}

}

void AssetBrowser::imgui_draw()
{
	if (!ImGui::Begin("Asset Browser")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | 
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	
	//if (set_keyboard_focus) {
	//	ImGui::SetKeyboardFocusHere();
	//	set_keyboard_focus = false;
	//}
	static bool match_case = false;
	ImGui::SetNextItemWidth(200.0);
	ImGui::InputTextWithHint("FILTER", "filter asset path", asset_name_filter, 256);
	ImGui::SameLine();
	ImGui::Checkbox("MATCH CASE", &match_case);
	const int name_filter_len = strlen(asset_name_filter);
	
	if (ImGui::SmallButton("Type filters..."))
		ImGui::OpenPopup("type_popup_assets");
	if (ImGui::BeginPopup("type_popup_assets"))
	{
		bool is_hiding_all = filter_type_mask != 0;
		if (ImGui::Checkbox("Show/Hide all", &is_hiding_all)) {
			if (filter_type_mask != 0)
				filter_type_mask = 0;
			else
				filter_type_mask = -1;
		}

		ImGui::CheckboxFlags("Materials", &filter_type_mask, (int)EdResType::Material);
		ImGui::CheckboxFlags("Models", &filter_type_mask, (int)EdResType::Model);
		ImGui::CheckboxFlags("Schemas", &filter_type_mask, (int)EdResType::Schema);
		ImGui::EndPopup();
	}

	
	std::string all_lower_cast_filter_name;
	if (!match_case) {
		all_lower_cast_filter_name = asset_name_filter;
		for (int i = 0; i < name_filter_len; i++) 
			all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
	}
	
	if (ImGui::BeginTable("Browser", 2, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);

		if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
			if (sorts_specs->SpecsDirty)
			{
				std::sort(all_resources.begin(), all_resources.end(),
					[&](const unique_ptr<EdResourceBase>& a, const unique_ptr<EdResourceBase>& b) -> bool {
						if(sorts_specs->Specs[0].ColumnIndex==1 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending)
							return a->get_asset_name() > b->get_asset_name();
						else if (sorts_specs->Specs[0].ColumnIndex == 1 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Descending)
							return a->get_asset_name() < b->get_asset_name();
						else if (sorts_specs->Specs[0].ColumnIndex == 0 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending)
							return a->get_type_name() > b->get_type_name();
						else if (sorts_specs->Specs[0].ColumnIndex == 0 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Descending)
							return a->get_type_name() < b->get_type_name();
						return true;
					}
				);
				sorts_specs->SpecsDirty = false;
			}


		ImGui::TableHeadersRow();
	
		for (int row_n = 0; row_n < all_resources.size(); row_n++)
		{
			auto res = all_resources[row_n].get();
			if (!should_type_show(res->get_type()))
				continue;
			if (!match_case && name_filter_len>0) {
				std::string path = res->get_full_path();
				for (int i = 0; i < path.size(); i++) path[i] = tolower(path[i]);
				if (path.find(all_lower_cast_filter_name, 0) == std::string::npos)
					continue;
			}
			else if(name_filter_len>0){
				if (res->get_full_path().find(asset_name_filter) == std::string::npos)
					continue;
			}
			ImGui::PushID(res);
			const bool item_is_selected = res == selected_resoruce;
	
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
	
			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
				selected_resoruce = res;
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				ImGui::SetDragDropPayload("AssetBrowserDragDrop",&res,sizeof(EdResourceBase*));
				
				ImGui::TextColored(color32_to_imvec4(res->get_asset_color()),"%s", res->get_type_name());
				ImGui::Text("Asset: %s", res->get_asset_name().c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Text(res->get_full_path().c_str());
			ImGui::TableNextColumn();
			ImGui::TextColored(color32_to_imvec4(res->get_asset_color()),  res->get_type_name());
	
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

const View_Setup& EditorDoc::get_vs()
{
	return vs_setup;
}

bool ConnectionList::imgui_draw_header(int index)
{
	std::vector<SignalProperty>* array_ = (std::vector<SignalProperty>*)prop->get_ptr(instance);
	ASSERT(index >= 0 && index < array_->size());
	SignalProperty& prop_ = array_->at(index);
	bool open = ImGui::TreeNode("##header");
	if (open)
		ImGui::TreePop();
	ImGui::SameLine(0);

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5,0.5,0.5,1.0));
	ImGui::Text(prop_.signal_name.c_str());
	ImGui::PopStyleColor();

	return open;
}
 void ConnectionList::imgui_draw_closed_body(int index)
{
	std::vector<SignalProperty>* array_ = (std::vector<SignalProperty>*)prop->get_ptr(instance);
	ASSERT(index >= 0 && index < array_->size());
	SignalProperty& prop_ = array_->at(index);

	ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 153, 152, 156 }));
	ImGui::Text("%s -> %s",prop_.target_name.c_str(), prop_.event_name.c_str());
	ImGui::PopStyleColor();
}


class IDictEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	EditorNode* get_node() {
		return (EditorNode*)instance;
	}

	virtual bool can_reset() override  { 
		auto node = get_node();

		auto vec = parse_hint_string(prop->range_hint);
		if (vec.empty() || vec[0].is_empty())
			return false;
		return (!vec[0].cmp(node->get_dict().get_string(prop->name)));
	}
	virtual void reset_value() override final {
		auto node = get_node();
		auto vec = parse_hint_string(prop->range_hint);
		assert(!vec.empty()&&!vec[0].is_empty());
		node->get_dict().set_string(prop->name, to_std_string_sv(vec[0]).c_str());
	}
	virtual void internal_update() override {
		auto node = get_node();
		if (!node->get_dict().has_key(prop->name)) {
			if (can_reset())
				reset_value();
		}
	}
};

class int_t_editor : public IDictEditor
{
	using IDictEditor::IDictEditor;

	// Inherited via IDictEditor
	virtual void internal_update() override {
		IDictEditor::internal_update();
		auto node = get_node();
		int i = node->get_dict().get_int(prop->name);
		auto vec = parse_hint_string(prop->range_hint);
		if (vec.size() == 3) {
			int min = atoi(vec[1].to_stack_string().c_str());
			int max = atoi(vec[2].to_stack_string().c_str());
			ImGui::SliderInt("##inputint", &i, min, max);
		}
		else
			ImGui::InputInt("##inputint", &i);
		node->get_dict().set_int(prop->name, i);
	}
};

class float_t_editor : public IDictEditor
{
	using IDictEditor::IDictEditor;

	// Inherited via IDictEditor
	virtual void internal_update() override {
		IDictEditor::internal_update();
		auto node = get_node();
		float f = node->get_dict().get_float(prop->name);
		auto vec = parse_hint_string(prop->range_hint);
		if (vec.size() == 3) {
			float min = atof(vec[1].to_stack_string().c_str());
			float max = atof(vec[2].to_stack_string().c_str());
			ImGui::SliderFloat("##inputfl", &f, min, max);
		}
		else
			ImGui::InputFloat("##inputfl", &f);
		node->get_dict().set_float(prop->name, f);
	}
};

class bool_t_editor : public IDictEditor
{
	using IDictEditor::IDictEditor;

	// Inherited via IDictEditor
	virtual void internal_update() override {
		IDictEditor::internal_update();
		auto node = get_node();
		bool b = node->get_dict().get_int(prop->name);
		auto vec = parse_hint_string(prop->range_hint);
		ImGui::Checkbox("##inputbool", &b);
		node->get_dict().set_int(prop->name, b);
	}
};

class vec3_t_editor : public IDictEditor
{
	using IDictEditor::IDictEditor;

	// Inherited via IDictEditor
	virtual void internal_update() override {
		IDictEditor::internal_update();
		auto node = get_node();
		glm::vec3 v = node->get_dict().get_vec3(prop->name);
		ImGui::DragFloat3("##vec3", &v.x,0.2);
		node->get_dict().set_vec3(prop->name, v);
	}

	virtual bool can_reset() override {
		auto node = get_node();
		auto vec = parse_hint_string(prop->range_hint);
		if (vec.empty() || vec[0].is_empty())
			return false;
		glm::vec3 v;
		int fields = sscanf(vec[0].to_stack_string().c_str(), "%f %f %f", &v.x, &v.y, &v.z);
		glm::vec3 dv = node->get_dict().get_vec3(prop->name);

		return glm::dot(v - dv, v - dv) > 0.00001;
	}
};

class color32_t_editor : public IDictEditor
{
	using IDictEditor::IDictEditor;

	// Inherited via IDictEditor
	virtual void internal_update() override {
		IDictEditor::internal_update();
		auto node = get_node();
		Color32 color = node->get_dict().get_color(prop->name);
		ImVec4 colorvec4 = ImGui::ColorConvertU32ToFloat4(color.to_uint());
		ImGui::ColorEdit4("##colorpicker", &colorvec4.x);
		uint32_t res = ImGui::ColorConvertFloat4ToU32(colorvec4);
		// im lazy
		color = *(Color32*)(&res);
		node->get_dict().set_color(prop->name, color);
	}
};


struct AutoStruct_asdf134 {
 AutoStruct_asdf134() {
	 auto& pfac = get_property_editor_factory();

	 pfac.registerClass<float_t_editor>("Leveled_float");
	 pfac.registerClass<int_t_editor>("Leveled_int");
	 pfac.registerClass<bool_t_editor>("Leveled_bool");
	 pfac.registerClass<color32_t_editor>("Leveled_color32");
	 pfac.registerClass<vec3_t_editor>("Leveled_vec3");

	 auto& afac = get_array_header_factory();

	 afac.registerClass<ConnectionList>("LevelEd_ConnectionList");

	 auto& sfac = get_property_serializer_factory();
 }
};
static AutoStruct_asdf134 AutoStruct_asdf134asdfa;

void ObjectOutliner::draw_table_R(EditorNode* node, int depth)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::Dummy(ImVec2(depth*5.f, 0));
	ImGui::SameLine();

	ImGui::PushID(node);
	{
		const bool item_is_selected = node == selected && node != nullptr;
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			selected = node;
		}
	}
	
	ImGui::SameLine();

	if (node == nullptr) {
		ImGui::Text(ed_doc.get_full_output_path());
	}
	else {
		ImGui::Text(node->get_schema_name());
	}

	if (node == nullptr) {
		for (int i = 0; i < ed_doc.nodes.size(); i++) {
			if (ed_doc.nodes[i])
				draw_table_R(ed_doc.nodes[i].get(), depth + 1);
		}
	}
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

		draw_table_R(nullptr, 0);

		ImGui::EndTable();
	}
	ImGui::End();
}

void EdPropertyGrid::draw()
{
	if (ImGui::Begin("Properties")) {
		if (node) {
			grid.update();
			if (!connection_props_from_node.props.empty())
				connection_grid.update();
		}
		else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();
}

void EdPropertyGrid::set(EditorNode* node_new)
{
	if (this->node == node_new)
		return;
	this->node = node_new;

	grid.clear_all();
	connection_grid.clear_all();
	connection_props_from_node = display_grid();
	props_from_node = display_grid();

	if (!node)
		return;
	if (!node->template_class) {
		node = nullptr;
		return;
	}

	for (const auto& prop : node->template_class->properties) {
		if (prop.dont_expose)
			continue;
		PropertyInfo inf;
		// these hold ptrs to std::strings, make sure template_class never gets updated without reseting this
		inf.custom_type_str = prop.type.c_str();
		inf.flags = PROP_DEFAULT;
		inf.type = core_type_id::Struct;
		inf.name = prop.name.c_str();
		inf.range_hint = prop.hint.c_str();
		inf.tooltip = prop.tooltip.c_str();
		props_from_node.props.push_back(inf);
	}
	props_from_node.list.list = props_from_node.props.data();
	props_from_node.list.count = props_from_node.props.size();
	props_from_node.list.type_name = node->template_class->name.c_str();

	grid.add_property_list_to_grid(&props_from_node.list, node);

	if (node->template_class->connections.signals.size() > 0) {
		PropertyInfo signal_list_prop;
		signal_list_prop.custom_type_str = "LevelEd_ConnectionList";
		signal_list_prop.flags = PROP_DEFAULT;
		signal_list_prop.tooltip = "in/out functions, this specifies what events to call on the target object when the entity emits the signal";
		signal_list_prop.name = "Connections";
		signal_list_prop.list_ptr = &callback_for_signals;
		signal_list_prop.offset = offsetof(EditorNode, signals);
		signal_list_prop.type = core_type_id::List;

		connection_props_from_node.props.push_back(signal_list_prop);
		connection_props_from_node.list.list = connection_props_from_node.props.data();
		connection_props_from_node.list.count = connection_props_from_node.props.size();
		connection_props_from_node.list.type_name = "Signals";

		connection_grid.add_property_list_to_grid(&connection_props_from_node.list, node,PG_LIST_PASSTHROUGH);
	}
}