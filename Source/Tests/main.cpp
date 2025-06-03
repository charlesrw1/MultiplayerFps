#include <iostream>
#include "Unittest.h"
#include "Framework/Util.h"
#include "LevelEditor/ObjectOutlineFilter.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "LevelSerialization/SerializationAPI.h"

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
				set_test_failed("<unknown>", "threw an exception");
		}
		is_in_test = false;

		if (!test_failed) {
			if (print_good)
				sys_print(Info, "good   %s\n", allTests[i].category);
		}
		else {
			sys_print(Error, "FAILED %s (%s:%s)\n", allTests[i].category,expression, reason);
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
	TEST_TRUE(OONameFilter::is_in_string("mesh", "MeshComponent"));

	auto out = OONameFilter::parse_into_and_ors("a | b c");
	TEST_TRUE(out.size() == 2 && out[0].size() == 1);
	TEST_TRUE(out[0][0] == "a");
	TEST_TRUE(out[1][0] == "b");
	out = OONameFilter::parse_into_and_ors("ab|c d e");
	TEST_TRUE(out.size() == 2);
	TEST_TRUE(out[0][0] == "ab");
	TEST_TRUE(out[1][0] == "c");
	TEST_TRUE(out[1][1] == "d");
}

ADD_TEST(object_outliner_filter_entity)
{
	Entity e;
	e.add_component_from_unserialization(new Component);
	e.set_editor_name("HELLO");
	PrefabAsset b;
	b.editor_set_newly_made_path("myprefab.pfb");
	e.what_prefab = &b;
	TEST_TRUE(OONameFilter::does_entity_pass_one_filter("myprefab", &e));
	TEST_TRUE(OONameFilter::does_entity_pass_one_filter("hel", &e));
	TEST_TRUE(OONameFilter::does_entity_pass_one_filter("com", &e));
	auto out = OONameFilter::parse_into_and_ors("bruh | hel com");
	TEST_TRUE(OONameFilter::does_entity_pass(out, &e));
	out = OONameFilter::parse_into_and_ors("bruh | hel abc");
	TEST_TRUE(!OONameFilter::does_entity_pass(out, &e));

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

static InterfaceDef A_interface_typeinfos[] = {
	InterfaceDef(&IMyInterface::typeinfo,compute_ptr_off<IMyInterface,AObj>())
};




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
	//
}


int main(int argc, char**argv)
{
	return !ProgramTester::get().run_all(true);
}