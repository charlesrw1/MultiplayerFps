#include "Level.h"
#include "Model.h"
#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>
static const char* const level_directory = "Data\\Models\\";
static std::array<Level*, 2> loaded_levels;


// Model.cpp
extern void add_node_mesh_to_model(Model* model, tinygltf::Model& inputMod, tinygltf::Node& node,
	std::map<int, int>& buffer_view_to_buffer, bool render_default, bool collide_default, std::vector<Game_Shader*>& mm,
	Physics_Mesh* physics, const glm::mat4& phys_transform);
extern void load_model_materials(std::vector<Game_Shader*>& materials, const std::string& fallbackname, tinygltf::Model& scene);


void parse_entity(Level* level, const tinygltf::Node& node, glm::mat4 transform)
{
	Level::Entity_Spawn es;
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
			else
				level->linked_meshes.push_back(m);
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

void init_collision_bvh(Level* level)
{
	std::vector<Bounds> bound_vec;
	Physics_Mesh& pm = level->collision;
	for (int i = 0; i < pm.tris.size(); i++) {
		Physics_Triangle& tri = pm.tris[i];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = pm.verticies[tri.indicies[i]];
		Bounds b(corners[0]);
		b = bounds_union(b, corners[1]);
		b = bounds_union(b, corners[2]);
		b.bmin -= glm::vec3(0.01);
		b.bmax += glm::vec3(0.01);

		bound_vec.push_back(b);
	}

	float time_start = GetTime();
	level->static_geo_bvh = BVH::build(bound_vec, 1, BVH_SAH);
	printf("Built world bvh in %.2f seconds\n", (float)GetTime() - time_start);
}


Level* LoadLevelFile(const char* level_name)
{
	for (int i = 0; i < loaded_levels.size(); i++) {
		if (loaded_levels[i]&&loaded_levels[i]->name == level_name) {
			loaded_levels[i]->ref_count++;
			return loaded_levels[i];
		}
	}
	if (loaded_levels[0] && loaded_levels[1])
		Fatalf("attempting to load 3 levels into memory");
	int open_slot = (loaded_levels[0]) ? 1 : 0;

	std::string path;
	path.reserve(256);
	path += level_directory;
	path += level_name;

	tinygltf::Model scene;
	tinygltf::TinyGLTF loader;
	std::string errStr;
	std::string warnStr;
	bool res = loader.LoadBinaryFromFile(&scene, &errStr, &warnStr, path);
	if (!res) {
		printf("Couldn't load level: %s\n", path.c_str());
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

	init_collision_bvh(level);

	level->ref_count = 1;
	loaded_levels[open_slot] = level;
	return level;
}

void FreeLevel(const Level* level)
{
	if (!level) return;
	ASSERT(level->ref_count != 0);
	int i = 0;
	for (; i < loaded_levels.size(); i++) {
		if (loaded_levels[i] == level)
			break;
	}
	if (i == loaded_levels.size())
		Fatalf("free called on already freed level");
	loaded_levels[i]->ref_count--;
	if (loaded_levels[i]->ref_count <= 0) {
		printf("deleting level, %s\n", loaded_levels[i]->name.c_str());
		delete loaded_levels[i];
		loaded_levels[i] = nullptr;
	}
}