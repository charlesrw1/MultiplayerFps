#include "Level.h"
#include "Model.h"
#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
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
	es.classname = node.extras.Get("classname").Get<std::string>();
	es.name = node.name;
	es.position = transform[3];
	es.rotation = glm::vec3(0.f);
	if (node.rotation.size() == 4) {
		glm::quat quat = glm::make_quat<double>(node.rotation.data());
		es.rotation = glm::eulerAngles(quat);
	}
	es.scale = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	
	auto keys = node.extras.Keys();
	for (int i = 0; i < keys.size(); i++) {
		int s = es.key_values.size();
		es.key_values.resize(s + 1);
		
		auto& kv = es.key_values.at(s);
		kv.push_back(keys.at(i));
		auto& v = node.extras.Get(keys.at(i));
		kv.push_back(v.Get<std::string>());
	}

	if (node.mesh != -1) {
		int s = es.key_values.size();
		es.key_values.resize(s + 1);

		auto& kv = es.key_values.at(s);
		kv.push_back("linked_mesh");
		kv.push_back(std::to_string(level->linked_meshes.size()));
	}

	level->espawns.push_back(es);
}

void add_level_light(Level* l, tinygltf::Model& scene, glm::mat4 transform, int index)
{
	Level_Light ll;
	tinygltf::Light& light = scene.lights.at(index);
	ll.position = transform[3];
	ll.direction = -transform[2];	// fixme
	ll.type = LIGHT_POINT;
	ll.color = glm::vec3(light.color[0], light.color[1], light.color[2]) * (float)light.intensity;
	if (light.type == "directional") {
		ll.type = LIGHT_DIRECTIONAL;
	}
	else if (light.type == "point") {
		
	}
	else if (light.type == "spot"){
		ll.type = LIGHT_SPOT;
		ll.spot_angle = light.spot.outerConeAngle;
	}
	else {
		sys_print("bad light type %s", light.type.c_str());
	}
	l->lights.push_back(ll);
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
		add_level_light(level, scene, global_transform, light_index);
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
			Level::StaticInstance sm;
			sm.model_index = level->static_meshes.size();
			sm.transform = global_transform;
			
			Model* m = new Model;
			std::map<int, int> mapping;
			add_node_mesh_to_model(m, scene, node, mapping, true, true, mm, &level->collision, global_transform);
			level->static_meshes.push_back(m);
			
			sm.collision_only = m->collision && m->parts.size() == 0;
			level->instances.push_back(sm);
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

void load_level_lights(Level* l, tinygltf::Model& scene)
{
	for (int i = 0; i < scene.lights.size(); i++) {
		tinygltf::Light& light = scene.lights[i];
		
		Level_Light ll;

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

	// load ents.txt

	std::string lightmap_path = map_dir + "lightmap.hdr";
	level->lightmap = mats.find_texture(lightmap_path.c_str(), false, true);

	level->collision.build();

	return level;
}

void FreeLevel(const Level* level)
{
	delete level;
}