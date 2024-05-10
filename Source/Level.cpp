#include "Level.h"
#include "Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Physics.h"
#include "Texture.h"
#include "Framework/Key_Value_File.h"
#include <array>

static const char* const maps_directory = "./Data/Maps/";

// to not go insane, this creates an object dictionary which is then loaded later
void create_light_obj_from_gltf(Level* level, cgltf_node* node, glm::mat4 transform)
{
	cgltf_light* light = node->light;

	Level::Entity_Spawn espawn;
	espawn.classname = "light";
	espawn.name = node->name;
	espawn.position = transform[3];
	espawn.rotation = -transform[2];	// FIXME
	
	const char* types[] = {
		"invalid","directional","point","spot"
	};

	Level_Light ll;
	espawn.spawnargs.set_string("type", types[light->type]);
	glm::vec3 color = glm::vec3(light->color[0], light->color[1], light->color[2]) * (float)light->intensity;
	espawn.spawnargs.set_vec3("color", color);
	
	if (light->type == cgltf_light_type_spot) {
		espawn.spawnargs.set_float("outer_cone", light->spot_inner_cone_angle);
		espawn.spawnargs.set_float("inner_cone", light->spot_outer_cone_angle);
	}

	espawn.spawnargs.set_int("?embedded", 1);

	level->espawns.push_back(espawn);
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

			smo.handle = idraw->register_obj();
			Render_Object rop;
			rop.mesh = &smo.model->mesh;
			rop.transform = smo.transform;
			rop.animator = nullptr;
			rop.visible = true;
			rop.mats = &smo.model->mats;
			idraw->update_obj(smo.handle, rop);

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


void on_node_callback(void* user, cgltf_data* data, cgltf_node* node, glm::mat4 global_transform)
{
	if (node->light) {
		create_light_obj_from_gltf((Level*)user, node, global_transform);
	}
}
#include "Types.h"

Level* open_empty_level()
{
	Level* level = new Level;
	level->name = "_Empty";

	return level;
}

Level* LoadLevelFile(const char* level_name)
{
	std::string map_dir;
	map_dir.reserve(256);

	map_dir += maps_directory;
	map_dir += level_name;
	map_dir += "/";
	std::string levelmesh_path = map_dir + "levelmesh.glb";

	File_Buffer* infile = Files::open(levelmesh_path.c_str());
	if (!infile) {
		printf("no level with such name\n");
		return nullptr;
	}
	Files::close(infile);

	Level* level = new Level;
	level->name = level_name;

	Prefab_Model* prefab = mods.find_or_load_prefab(levelmesh_path.c_str(), true, on_node_callback, level);
	if (!prefab) {
		delete level;
		return nullptr;
	}
	level->level_prefab = prefab;
	
	load_ents(level, map_dir);
	create_statics_from_dicts(level);

	std::string lightmap_path = map_dir + "lightmap.hdr";
	level->lightmap = mats.find_texture(lightmap_path.c_str(), false, true);


	level->collision = std::move(level->level_prefab->physics);
	level->collision->build();


	for (int i = 0; i < prefab->nodes.size(); i++) {
		auto& node = prefab->nodes[i];
		handle<Render_Object> handle = idraw->register_obj();
		Render_Object obj;
		obj.mesh = &prefab->meshes[node.mesh_idx];
		obj.transform = node.transform;
		obj.animator = nullptr;
		obj.visible = true;
		obj.mats = &prefab->mats;
		
		idraw->update_obj(handle, obj);
		level->prefab_handles.push_back(handle);
	}

	return level;
}

void FreeLevel(Level* level)
{
	if (level) {
		for (int i = 0; i < level->prefab_handles.size(); i++)
			idraw->remove_obj(level->prefab_handles[i]);

		for (int i = 0; i < level->static_mesh_objs.size(); i++)
			idraw->remove_obj(level->static_mesh_objs[i].handle);

		mods.free_prefab(level->level_prefab);
		delete level;
		mods.compact_memory();
	}
}