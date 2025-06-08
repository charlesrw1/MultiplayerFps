#include <iostream>
#include "Unittest.h"
#include "Framework/Util.h"
#include "LevelEditor/ObjectOutlineFilter.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "LevelSerialization/SerializationAPI.h"

#include "Framework/Files.h"

std::string UnitTestUtil::get_text_of_file(const std::string& path)
{
	auto file = FileSys::open_read_engine(path.c_str());
	if (!file)
		throw std::runtime_error("File not found " + path);
	std::string str(file->size(), ' ');
	file->read(&str[0], str.size());
	return str;
}


bool ProgramTester::run_all(bool print_good)
{
	sys_print(Info, "--------- Running Tests ----------\n");
	sys_print(Info, "num tests: %d\n", (int)allTests.size());
	int er = 0;
	for (int i = 0; i < allTests.size(); i++) {
		test_failed = false;
		expression = reason = "";

		is_in_test = true;
		try {
			allTests[i].func();
		}
		catch (std::runtime_error er) {
			if (!test_failed)
				set_test_failed("exception", er.what());
		}
		is_in_test = false;

		if (!test_failed) {
			if (print_good)
				sys_print(Info, "good   %s\n", allTests[i].category);
		}
		else {
			sys_print(Error, "FAILED %s (%s:%s)\n", allTests[i].category,expression.c_str(), reason.c_str());
			er++;
		}
	}
	if (er == 0)
		sys_print(Info, "all tests passed\n");
	else
		sys_print(Error, "tests had %d errors\n", er);
	return er == 0;
}

ADD_TEST(object_outliner_filter)
{
	checkTrue(OONameFilter::is_in_string("mesh", "MeshComponent"));

	auto out = OONameFilter::parse_into_and_ors("a | b c");
	checkTrue(out.size() == 2 && out[0].size() == 1);
	checkTrue(out[0][0] == "a");
	checkTrue(out[1][0] == "b");
	out = OONameFilter::parse_into_and_ors("ab|c d e");
	checkTrue(out.size() == 2);
	checkTrue(out[0][0] == "ab");
	checkTrue(out[1][0] == "c");
	checkTrue(out[1][1] == "d");
}

ADD_TEST(object_outliner_filter_entity)
{
	Entity e;
	e.add_component_from_unserialization(new Component);
	e.set_editor_name("HELLO");
	PrefabAsset b;
	b.editor_set_newly_made_path("myprefab.pfb");
	e.what_prefab = &b;
	checkTrue(OONameFilter::does_entity_pass_one_filter("myprefab", &e));
	checkTrue(OONameFilter::does_entity_pass_one_filter("hel", &e));
	checkTrue(OONameFilter::does_entity_pass_one_filter("com", &e));
	auto out = OONameFilter::parse_into_and_ors("bruh | hel com");
	checkTrue(OONameFilter::does_entity_pass(out, &e));
	out = OONameFilter::parse_into_and_ors("bruh | hel abc");
	checkTrue(!OONameFilter::does_entity_pass(out, &e));

}
class A {
public:
	virtual void do_something() {
		printf("doing something\n");
	}
	int a=1, b, c;
};
class B {
public:
	virtual void eat_dinner() {
		printf("eating dinner\n");
	}
	int d=2, e, f;
};
class C : public A,public B {
public:
	void eat_dinner() override {
		printf("eating desert\n");
	}
	void do_something() override {
		printf("not doing anything now\n");
	}
	int g=3, h, i;
};

struct InterfaceTypeInfo {
	const char* name = "";
	int id = 0;
	// function property infos
};
#define INTERFACE_BODY() \
	static InterfaceTypeInfo typeinfo;
class IMyInterface {
public:
	INTERFACE_BODY();

	virtual void do_stuff() = 0;
};
class IEatable {
public:
	INTERFACE_BODY();

	virtual void do_stuff() = 0;
};
template<typename Derived, typename Base>
static ptrdiff_t compute_ptr_off()
{
	Derived* derivedPtr = (Derived*)1;
	Base* basePtr = static_cast<Base*>(derivedPtr);
	return (intptr_t)basePtr - (intptr_t)derivedPtr;
}
struct InterfaceDef {
	InterfaceDef(InterfaceTypeInfo* typeinfo, int offset)
		: offset(offset), info(typeinfo) {}
	InterfaceTypeInfo* info = nullptr;
	int offset = 0;
};

class BaseObject {

};
class AObj : public BaseObject, public IMyInterface
{

};

//static InterfaceDef A_interface_typeinfos[] = {
//	InterfaceDef(&IMyInterface::typeinfo,compute_ptr_off<IMyInterface,AObj>())
//};




ADD_TEST(multiple_inheritance)
{
	
//	auto dif2 = ComputePointerOffset<C, B>;

}
#include "Game/Components/MeshComponent.h"
#include "Render/MaterialPublic.h"
ADD_TEST(assets)
{
	MeshComponent c;
	auto inst = new MaterialInstance();
	inst->editor_set_newly_made_path("something/testgrid.mm");
	c.set_material_override(inst);
}




int main(int argc, char**argv)
{
	FileSys::init();
	ClassBase::init_class_reflection_system();

	return !ProgramTester::get().run_all(true);
}