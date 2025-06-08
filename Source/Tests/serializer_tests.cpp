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
	TopDownPlayer obj;
	checkTrue(obj.is_a<ClassBase>());
	obj.using_third_person_movement = true;
	obj.myarray = { 1,2,3,4 };
	obj.fov = 10.0;

	WriteSerializerBackendJson backend;
	ClassBase* ptr = &obj;
	backend.serialize_class("myobj", TopDownPlayer::StaticType, ptr);

	std::string name = "Charlie";
	int age = 22;
	glm::vec3 pos(1.f,2.f,3.f);
	backend.serialize("name", name);
	backend.serialize("age", age);
	backend.serialize("pos", pos);
	int sz = 0;
	backend.serialize_dict("obj");
	{
		backend.serialize_array("nums", sz);
		for (int i = 0; i < 10; i++)
			backend.serialize_ar(i);
		backend.end_obj();
	}
	backend.end_obj();
	checkTrue(backend.stack.size() == 1);
	std::cout << backend.obj.dump(1)<<'\n';

	ReadSerializerBackendJson read(backend.obj.dump());
	string inname;
	int inage{};

	read.serialize("name", inname);
	checkTrue(inname == "Charlie");
	read.serialize("age", inage);
	checkTrue(inage == 22);
	glm::vec3 inpos{};
	read.serialize("pos", inpos);
	checkTrue(vec_equals(inpos, glm::vec3(1.f,2.f,3.f)));
	bool b = read.serialize_dict("obj");
	checkTrue(b);
	{
		b = read.serialize_array("nums", sz);
		checkTrue(b && sz == 10);
		for (int i = 0; i < sz; i++) {
			int ini = 0;
			read.serialize_ar(ini);
			checkTrue(ini == i);
		}
		read.end_obj();
	}
	read.end_obj();

	ptr = nullptr;
	read.serialize_class("myobj", TopDownPlayer::StaticType, ptr);
	checkTrue(ptr);
	auto asplayer = ptr->cast_to<TopDownPlayer>();
	checkTrue(asplayer);
	checkTrue(asplayer->using_third_person_movement == obj.using_third_person_movement);
	checkTrue((asplayer->myarray == obj.myarray));
	checkTrue(glm::abs(asplayer->fov- obj.fov) <0.001)
}