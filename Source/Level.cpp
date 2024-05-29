#include "Level.h"
#include "Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>

static const char* const maps_directory = "./Data/Maps/";

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



handle<Render_Object> make_static_mesh_from_dict(const Dict& dict)
{

	const char* get_str = "";

	if (*(get_str = dict.get_string("model", "")) != 0) {
		
		Model* m = mods.find_or_load(get_str);
		if (m) {
			glm::mat4 transform = glm::translate(glm::mat4(1), dict.get_vec3("position"));
			glm::vec3 angles = dict.get_vec3("rotation");
			transform = transform * glm::eulerAngleXYZ(angles.x,angles.y,angles.z);
			transform = glm::scale(transform, dict.get_vec3("scale",glm::vec3(1.f)));


			auto handle = idraw->register_obj();
			Render_Object rop;
			rop.model = m;
			rop.transform = transform;
			rop.animator = nullptr;
			rop.visible = true;
			rop.shadow_caster = dict.get_int("casts_shadows", 1);
			idraw->update_obj(handle, rop);

			return handle;
		}
	}

	return {};
}

handle<Render_Light> make_light_from_dict(const Dict& dict)
{


	const char* type = dict.get_string("type", "point");
	glm::vec3 color = dict.get_vec3("color", glm::vec3(1.f));
	Render_Light light;
	light.color = color;
	light.position = dict.get_vec3("position");
	glm::vec3 angles = dict.get_vec3("angles");
	light.normal = AnglesToVector(angles.x, angles.y);
	if (strcmp(type, "directional") == 0) {
		light.type = LIGHT_DIRECTIONAL;
		light.main_light_override = true;
	}
	else if (strcmp(type, "point") == 0) {
		light.type = LIGHT_POINT;
	}
	else {
		light.type = LIGHT_SPOT;
		light.conemin = dict.get_float("outer_cone", 0.7f);
	}

	auto handle = idraw->register_light(light);
	return handle;
}



#include "Framework/Files.h"


bool MapLoadFile::parse(const std::string name)
{
	spawners.clear();
	mapname = "";

	std::string mappath =  maps_directory;
	mappath += name;
	mappath += "/entities.txt";

	DictParser parser;
	bool good = parser.load_from_file(name.c_str());
	if (!good) {
		sys_print("!!! couldn't find ent file %s\n", mappath.c_str());
		return false;
	}

	while (!parser.is_eof()) {

		Dict d;
		parser.expect_item_start();
		StringView tok;
		while (parser.read_string(tok) && !parser.check_item_end(tok) && !parser.is_eof()) {
			std::string key = std::string(tok.str_start, tok.str_len);
			parser.read_string(tok);
			std::string val = std::string(tok.str_start, tok.str_len);
			d.keyvalues.insert({ std::move(key),std::move(val) });
		}

		spawners.push_back(std::move(d));
	}

	mapname = name;
	return parser.is_eof();
}
#include "Framework/DictWriter.h"
#include <fstream>
void MapLoadFile::write_to_disk(const std::string name)
{
	sys_print("*** Writing map %s to disk\n", name.c_str());
	sys_print("   -> num ents: %s\n", (int) spawners.size());

	std::string mappath = maps_directory;
	mappath += name;
	mappath += "/entities.txt";

	DictWriter out;

	for (auto& s : spawners) {

		out.write_item_start();
		for (auto& kv : s.dict.keyvalues) {

			out.write_key(kv.first.c_str());
			out.write_value(kv.second.c_str());
		}
		out.write_item_end();
	}

	std::ofstream outfile(mappath);
	size_t count = out.get_output().size();
	outfile.write(out.get_output().c_str(), count);
	outfile.close();
}

bool Level::open_from_file(const std::string& path)
{
	bool b = loadfile.parse(path);
	if (!b)
		return false;

	for (int i = 0; i < loadfile.spawners.size(); i++) {
		auto& spawner = loadfile.spawners[i];
		if (spawner.type == NAME("static_mesh"))
			smeshes.push_back(make_static_mesh_from_dict(spawner.dict));
		else if (spawner.type == NAME("static_light"))
			slights.push_back(make_light_from_dict(spawner.dict));

	}
}

void Level::free_level()
{
	for (int i = 0; i < smeshes.size(); i++)
		idraw->remove_obj(smeshes[i]);

	for (int i = 0; i < slights.size(); i++)
		idraw->remove_light(slights[i]);
}