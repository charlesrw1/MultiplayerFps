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

ADD_TEST(serializer)
{
	Entity e;
	e.set_ls_position(glm::vec3(5));
	ComponentWithStruct thing;
	thing.things.s = "Hello";
	thing.target = glm::vec3(3, 2, 1);
	thing.what = &e;
	TopDownPlayer obj;
	thing.player = &obj;
	checkTrue(obj.is_a<ClassBase>());
	obj.using_third_person_movement = true;
	obj.myarray = { 1,2,3,4 };
	obj.fov = 10.0;
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson backend(&pathmaker,&thing);

	std::cout << backend.obj.dump(-1)<<'\n';

	ReadSerializerBackendJson read(backend.obj.dump(),&pathmaker);
	
	ClassBase* ptr = read.rootobj;
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