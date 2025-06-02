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



int main(int argc, char**argv)
{
	return !ProgramTester::get().run_all(true);
}