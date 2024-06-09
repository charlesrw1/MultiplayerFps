#include "Level.h"
#include "Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>

#include "Physics/Physics2.h"

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



void make_static_mesh_from_dict(vector<handle<Render_Object>>& objs, vector<PhysicsActor*>& phys, const Dict& dict)
{
	const char* get_str = "";

	const char* modelname = dict.get_string("model");
	if (*modelname == 0) {
		return;
	}
	Model* model = mods.find_or_load(modelname);
	if (!model)
		return;

	glm::vec3 p = dict.get_vec3("position");
	glm::vec3 s = dict.get_vec3("scale",glm::vec3(1.f));
	glm::vec4 qvec = dict.get_vec4("rotation");
	glm::quat q = glm::quat(qvec.x, qvec.y, qvec.z, qvec.w);
	glm::mat4 transform = glm::translate(glm::mat4(1), p);
	transform *= glm::mat4_cast(q);
	transform = glm::scale(transform, s);

	Color32 color = dict.get_color("color");

	auto handle = idraw->register_obj();
	Render_Object rop;
	rop.param1 = color;
	rop.model = model;
	rop.transform = transform;
	rop.animator = nullptr;
	rop.visible = true;
	rop.shadow_caster = dict.get_int("casts_shadows", 1);

	bool has_collisions = dict.get_int("has_collisions", 1);
	idraw->update_obj(handle, rop);
	objs.push_back(handle);

	if (has_collisions) {
		// create physics
		PhysicsActor* actor = g_physics->allocate_physics_actor();
		assert(actor);
		actor->set_entity(nullptr);

		PhysTransform pt;
		pt.position = p;
		pt.rotation = q;

		glm::vec3 halfsize = (model->get_bounds().bmax - model->get_bounds().bmin) * 0.5f * glm::abs(s);
		glm::vec3 center = model->get_bounds().get_center() * s;
		center = glm::mat4_cast(q) * glm::vec4(center, 1.0);
		PhysTransform tr;
		tr.position = p;	// fixme: scaling for models
		tr.rotation = q;
		if (model->get_physics_body())
			actor->create_static_actor_from_model(model, tr);
		else
			actor->create_static_actor_from_shape(physics_shape_def::create_box(halfsize, p + center, q));
	
		phys.push_back(actor);
	}
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


	auto file = FileSys::open_read(name.c_str());
	if (!file) {
		sys_print("!!! couldn't find map file %s\n", name.c_str());
		return false;
	}
	DictParser parser;
	parser.load_from_file(file.get());

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
	sys_print("   num ents: %d\n", (int) spawners.size());

	DictWriter out;

	for (auto& s : spawners) {

		out.write_item_start();
		for (auto& kv : s.dict.keyvalues) {

			out.write_key(kv.first.c_str());
			out.write_value_quoted(kv.second.c_str());
		}
		out.write_item_end();
	}

	std::ofstream outfile(name);
	size_t count = out.get_output().size();
	outfile.write(out.get_output().c_str(), count);
	outfile.close();
}

bool Level::open_from_file(const std::string& path)
{
	std::string fullpath = maps_directory + path + ".txt";
	bool b = loadfile.parse(fullpath);
	if (!b)
		return false;

	for (int i = 0; i < loadfile.spawners.size(); i++) {
		auto& spawner = loadfile.spawners[i];
		if (strcmp(spawner.dict.get_string("_schema_name"), "StaticMesh") == 0)
			make_static_mesh_from_dict(smeshes, sphysics, spawner.dict);

	}
	return true;
}

void Level::free_level()
{
	for (int i = 0; i < smeshes.size(); i++)
		idraw->remove_obj(smeshes[i]);

	for (int i = 0; i < slights.size(); i++)
		idraw->remove_light(slights[i]);

	for (int i = 0; i < sphysics.size(); i++)
		g_physics->free_physics_actor(sphysics[i]);

	smeshes.clear();
	slights.clear();
	sphysics.clear();
}