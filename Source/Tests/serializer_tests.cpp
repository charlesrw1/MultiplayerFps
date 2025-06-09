#include "Unittest.h"
#include "Framework/Serializer.h"
#include "Framework/SerializerJson.h"
#include <iostream>
#include "Game/TopDownShooter/TopDownPlayer.h"

static bool vec_equals(const glm::vec3& v, glm::vec3 target)
{
	const float ep = 0.0001;
	return glm::abs(v.x - target.x) < ep && glm::abs(v.y - target.y) < ep && glm::abs(v.z - target.z) < ep;
}

ADD_TEST(diff_testing)
{
	using namespace nlohmann;
	json a;
	a["object"] = { 1,2,3,4 };
	json b;
	b["object"] = { 1,2 };
	auto diff = JsonSerializerUtil::diff_json(a,b);
	checkTrue((diff["object"] == std::vector<int>{1, 2}));
}

ADD_TEST(serializer)
{
	Entity e;
	e.set_ls_position(glm::vec3(5));
	ComponentWithStruct thing;
	thing.things.s = "Hello";
	thing.target = glm::vec3(3, 2, 1);
	thing.what = &e;
	TopDownPlayer obj;
	e.add_component_from_unserialization(&obj);
	e.add_component_from_unserialization(&thing);
	thing.player = &obj;
	checkTrue(obj.is_a<ClassBase>());
	obj.using_third_person_movement = true;
	obj.myarray = { 1,2,3,4 };
	obj.fov = 10.0;
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson backend(pathmaker,thing);

	// test diffing
	ComponentWithStruct default_thing;
	WriteSerializerBackendJson backend2(pathmaker, default_thing);
	//auto diff = JsonSerializerUtil::diff_json(backend2.obj["objs"]["1"], backend.obj["objs"]["1"]);
	//std::cout << "pre\n";
	//std::cout << backend.get_output().dump(1) << '\n';

	//backend.obj["objs"]["1"] = diff;
	//std::cout << "post\n";
	//std::cout << backend.obj.dump(-1)<<'\n';

	MakeObjectFromPathGeneric objmaker;
	ReadSerializerBackendJson read(backend.get_output().dump(), objmaker ,*AssetDatabase::loader);
	
	ClassBase* ptr = read.get_root_obj();
	checkTrue(ptr);
	auto ascomp = ptr->cast_to<ComponentWithStruct>();
	checkTrue(ascomp);
	checkTrue(ascomp->things.s == thing.things.s);
	checkTrue(vec_equals(ascomp->target, thing.target));
	auto asplayer = ascomp->player;
	checkTrue(asplayer);
	checkTrue(asplayer->using_third_person_movement == obj.using_third_person_movement);
	checkTrue((asplayer->myarray == obj.myarray));
	checkTrue(glm::abs(asplayer->fov- obj.fov) <0.001)
}