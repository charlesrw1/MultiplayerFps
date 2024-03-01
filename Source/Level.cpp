#include "Level.h"
#include "Model.h"
#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>

static const char* const maps_directory = "./Data/Maps/";

// Model.cpp
extern void add_node_mesh_to_model(Model* model, tinygltf::Model& inputMod, tinygltf::Node& node,
	std::map<int, int>& buffer_view_to_buffer, bool render_default, bool collide_default, std::vector<Game_Shader*>& mm,
	Physics_Mesh* physics, const glm::mat4& phys_transform);
extern void load_model_materials(std::vector<Game_Shader*>& materials, const std::string& fallbackname, tinygltf::Model& scene);

void parse_entity(Level* level, const tinygltf::Node& node, glm::mat4 transform)
{
	Level::Entity_Spawn es;
	es.spawnargs.set_string("classname", node.extras.Get("classname").Get<std::string>().c_str());
	es.classname = es.spawnargs.get_string("classname");
	es.name = node.name;
	es.position = transform[3];
	es.rotation = glm::vec3(0.f);
	if (node.rotation.size() == 4) {
		glm::quat quat = glm::make_quat<double>(node.rotation.data());
		es.rotation = glm::eulerAngles(quat);
	}
	es.scale = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	
	const auto& keys = node.extras.Keys();
	for (int i = 0; i < keys.size(); i++) {
		auto& v = node.extras.Get(keys.at(i));
		es.spawnargs.set_string(keys.at(i).c_str(), v.Get<std::string>().c_str());
	}

	if (node.mesh != -1) {
		es.spawnargs.set_int("linked_mesh", level->linked_meshes.size());
	}

	es.spawnargs.set_int("?embedded", 1);

	level->espawns.push_back(es);
}

// to not go insane, this creates an object dictionary which is then loaded later
void create_light_obj_from_gltf(Level* l, tinygltf::Model& scene, glm::mat4 transform, int index, const char* name)
{
	Level::Entity_Spawn espawn;
	espawn.classname = "light";
	espawn.name = name;
	espawn.position = transform[3];
	espawn.rotation = -transform[2];	// FIXME
	
	Level_Light ll;
	tinygltf::Light& light = scene.lights.at(index);
	espawn.spawnargs.set_string("type", light.type.c_str());
	glm::vec3 color = glm::vec3(light.color[0], light.color[1], light.color[2]) * (float)light.intensity;
	espawn.spawnargs.set_vec3("color", color);
	
	if (light.type == "spot") {
		espawn.spawnargs.set_float("outer_cone", light.spot.outerConeAngle);
		espawn.spawnargs.set_float("inner_cone", light.spot.innerConeAngle);
	}

	espawn.spawnargs.set_int("?embedded", 1);

	l->espawns.push_back(espawn);
}

static void traverse_tree(Level* level, tinygltf::Model& scene, tinygltf::Node& node, 
	glm::mat4 global_transform, std::vector<Game_Shader*>& mm)
{
	glm::mat4 local_transform = glm::mat4(1);
	if (node.matrix.size() == 16) {
		double* m = node.matrix.data();
		local_transform = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
	}
	else {
		glm::vec3 translation = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);
		glm::quat rot = glm::quat(1.f, 0.f, 0.f, 0.f);
		if (node.translation.size() != 0)
			translation = glm::make_vec3<double>(node.translation.data());
		if (node.rotation.size() != 0)
			rot = glm::make_quat<double>(node.rotation.data());
		if (node.scale.size() != 0)
			scale = glm::make_vec3<double>(node.scale.data());
		local_transform = glm::translate(glm::mat4(1), translation);
		local_transform = local_transform * glm::mat4_cast(rot);
		local_transform = glm::scale(local_transform, scale);
	}
	global_transform = global_transform * local_transform;

	bool is_entity = false;

	if (node.extensions.find("KHR_lights_punctual") != node.extensions.end()) {
		auto v = node.extensions.find("KHR_lights_punctual")->second.Get("light");
		int light_index = v.GetNumberAsInt();
		create_light_obj_from_gltf(level, scene, global_transform, light_index, node.name.c_str());
	}


	// if its an entity, default is no collision/rendering
	if (node.extras.IsObject() && node.extras.Has("classname")) {
		is_entity = true;

		parse_entity(level, node, global_transform);
	}

	if (node.mesh != -1) {
		// ASSUMPTION: (FIXME!!) meshes are only used by one node
		if (is_entity)
		{
			Model* m = new Model;
			std::map<int, int> mapping;
			add_node_mesh_to_model(m, scene, node, mapping, false, false, mm, nullptr, glm::mat4(1));
			if (!m->collision && m->parts.size() == 0)
				delete m;
			else {
				if (m->collision)
					m->collision->build();
				level->linked_meshes.push_back(m);
			}
		}
		else
		{
			Model* m = new Model;
			std::map<int, int> mapping;
			add_node_mesh_to_model(m, scene, node, mapping, true, true, mm, &level->collision, global_transform);
			level->static_meshes.push_back(m);
			
			if (m->parts.size() > 0) {
				Static_Mesh_Object smo;
				smo.model = m;
				smo.transform = global_transform;
				smo.is_embedded_mesh = true;
				level->static_mesh_objs.push_back(smo);
			}
		}
	}

	for (int i = 0; i < node.children.size(); i++)
		traverse_tree(level, scene, scene.nodes[node.children[i]], global_transform,mm);
}

static void map_materials_to_models(Level* level, tinygltf::Model& scene, std::vector<Game_Shader*>& mm)
{
	for (int i = 0; i < level->linked_meshes.size(); i++) {
		Model* m = level->linked_meshes.at(i);
		for (int i = 0; i < m->parts.size(); i++) {
			auto& part = m->parts[i];
			if (part.material_idx != -1) {
				m->materials.push_back(mm.at(part.material_idx));
				part.material_idx = m->materials.size() - 1;
			}
		}
	}
	for (int i = 0; i < level->static_meshes.size(); i++) {
		Model* m = level->static_meshes.at(i);
		for (int i = 0; i < m->parts.size(); i++) {
			auto& part = m->parts[i];
			if (part.material_idx != -1) {
				m->materials.push_back(mm.at(part.material_idx));
				part.material_idx = m->materials.size() - 1;
			}
		}
	}
}

void Physics_Mesh::build()
{
	std::vector<Bounds> bound_vec;
	for (int i = 0; i < tris.size(); i++) {
		Physics_Triangle& tri = tris[i];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = verticies[tri.indicies[i]];
		Bounds b(corners[0]);
		b = bounds_union(b, corners[1]);
		b = bounds_union(b, corners[2]);
		b.bmin -= glm::vec3(0.01);
		b.bmax += glm::vec3(0.01);

		bound_vec.push_back(b);
	}

	float time_start = GetTime();
	bvh = BVH::build(bound_vec, 1, BVH_SAH);
	printf("Built bvh in %.2f seconds\n", (float)GetTime() - time_start);
}

#include "Key_Value_File.h"
void load_ents(Level* l, const std::string& mapdir)
{
	std::string ent_file_name = mapdir + "ents.txt";
	Key_Value_File ent_file;
	bool good = ent_file.open(ent_file_name.c_str());
	if (!good) return;
	{
		for (auto& ent : ent_file.name_to_entry) {
			Level::Entity_Spawn espawn;
			espawn.spawnargs = std::move(ent.second.dict);
			espawn.classname = espawn.spawnargs.get_string("classname", "");
			if (espawn.classname.empty()) {
				sys_print("Entity without classname, skipping...\n");
				continue;
			}
			espawn.name = ent.first;
			espawn.position = espawn.spawnargs.get_vec3("position");
			espawn.rotation = espawn.spawnargs.get_vec3("rotation");
			espawn.scale = espawn.spawnargs.get_vec3("scale",glm::vec3(1.f));

			l->espawns.push_back(espawn);
		}
	}
}

Static_Mesh_Object make_static_mesh_from_dict(Level::Entity_Spawn* obj)
{
	Dict* dict = &obj->spawnargs;
	const char* get_str = "";

	if (*(get_str = dict->get_string("model", "")) != 0) {
		Static_Mesh_Object smo;
		smo.model = FindOrLoadModel(get_str);
		if (smo.model) {
			glm::mat4 transform = glm::translate(glm::mat4(1), obj->position);
			transform = transform * glm::eulerAngleXYZ(obj->rotation.x, obj->rotation.y, obj->rotation.z);
			transform = glm::scale(transform, obj->scale);

			smo.transform = transform;

			smo.casts_shadows = dict->get_int("casts_shadows", 1);

			return smo;
		}
	}

	return {};
}

Level_Light make_light_from_dict(Level::Entity_Spawn* obj)
{
	Dict* dict = &obj->spawnargs;

	const char* type = dict->get_string("type", "point");
	glm::vec3 color = dict->get_vec3("color", glm::vec3(1.f));
	Level_Light light;
	light.color = color;
	light.position = obj->position;
	light.direction = obj->rotation;
	if (strcmp(type, "directional") == 0) {
		light.type = LIGHT_DIRECTIONAL;
	}
	else if (strcmp(type, "point") == 0) {
		light.type = LIGHT_POINT;
	}
	else {
		light.type = LIGHT_SPOT;
		light.spot_angle = dict->get_float("outer_cone", 0.7f);
	}

	return light;
}

void create_statics_from_dicts(Level* level)
{
	const char* get_str = "";
	for (int i = 0; i < level->espawns.size(); i++)
	{
		Level::Entity_Spawn* obj = &level->espawns[i];
		Dict* dict = &obj->spawnargs;
		if (obj->classname == "static_mesh") {
			Static_Mesh_Object smo = make_static_mesh_from_dict(obj);
			if (smo.model) {
				level->static_mesh_objs.push_back(smo);
				obj->_ed_varying_index_for_statics = level->static_mesh_objs.size() - 1;
			}
		}
		else if (obj->classname == "light") {
			level->lights.push_back(make_light_from_dict(obj));
			obj->_ed_varying_index_for_statics = level->lights.size() - 1;
		}
		else if (obj->classname == "decal") {

		}
		else if (obj->classname == "static_sound") {

		}
		else if (obj->classname == "static_particle") {

		}
	}


}

Level* LoadLevelFile(const char* level_name)
{
	std::string map_dir;
	map_dir.reserve(256);

	map_dir += maps_directory;
	map_dir += level_name;
	map_dir += "/";

	std::string levelmesh_path = map_dir + "levelmesh.glb";
	tinygltf::Model scene;
	tinygltf::TinyGLTF loader;
	std::string errStr;
	std::string warnStr;
	File_Buffer* infile = Files::open(levelmesh_path.c_str());
	if (!infile) {
		printf("no level with such name\n");
		return nullptr;
	}
	bool res = loader.LoadBinaryFromMemory(&scene, &errStr, &warnStr,(unsigned char*)infile->buffer,infile->length);
	Files::close(infile);
	if (!res) {
		printf("Couldn't load level: %s\n", level_name);
		return nullptr;
	}

	Level* level = new Level;
	level->name = level_name;

	std::vector<Game_Shader*> mm;
	load_model_materials(mm, level->name, scene);

	tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	for (int i = 0; i < defscene.nodes.size(); i++) {
		traverse_tree(level, scene, scene.nodes[defscene.nodes[i]], glm::mat4(1), mm);
	}
	map_materials_to_models(level, scene, mm);
	load_ents(level, map_dir);
	
	create_statics_from_dicts(level);

	std::string lightmap_path = map_dir + "lightmap.hdr";
	level->lightmap = mats.find_texture(lightmap_path.c_str(), false, true);

	level->collision.build();

	return level;
}

void FreeLevel(const Level* level)
{
	delete level;
}