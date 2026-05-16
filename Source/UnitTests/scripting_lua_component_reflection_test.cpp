#include <gtest/gtest.h>
#include "Framework/ClassBase.h"
#include "Scripting/ScriptManager.h"
#include "Framework/ReflectionProp.h"
#include "Testheader.h"

// These tests cover the pieces of the Lua-component reflection pipeline that can
// run without engine init: parser → ParseProperty list, and the type-mapper +
// layout logic on LuaClassTypeInfo. The full alloc / serialize / reload-merge
// path is exercised by the integration test phase via Data/scripts/tests.

class LuaComponentReflectionTest : public ::testing::Test
{
protected:
	void SetUp() override {
		sm = new ScriptManager();
		ScriptManager::inst = sm;
	}
	void TearDown() override {
		delete sm;
		sm = nullptr;
		ScriptManager::inst = nullptr;
	}
	ScriptManager* sm = nullptr;
};

TEST_F(LuaComponentReflectionTest, ParserCapturesTypedFields) {
	auto out = ScriptLoadingUtil::parse_text("---@class MyClass\n"
											 "MyClass = {\n"
											 "  ---@type number\n"
											 "  hp = 100,\n"
											 "  ---@type boolean\n"
											 "  alive = true,\n"
											 "  ---@type string\n"
											 "  name = \"foo\",\n"
											 "}\n");
	ASSERT_EQ(out.size(), 1u);
	ASSERT_EQ(out[0].props.size(), 3u);
	EXPECT_EQ(out[0].props[0].name, "hp");
	EXPECT_EQ(out[0].props[0].type_str, "number");
	EXPECT_EQ(out[0].props[1].name, "alive");
	EXPECT_EQ(out[0].props[1].type_str, "boolean");
	EXPECT_EQ(out[0].props[2].name, "name");
	EXPECT_EQ(out[0].props[2].type_str, "string");
}

TEST_F(LuaComponentReflectionTest, ParserOnlyCapturesTypedFields) {
	// Fields without a preceding ---@type are dropped entirely so they never reach
	// synthesis. Lua scripts can still read/write them at runtime; they're just not
	// engine-reflected.
	auto out = ScriptLoadingUtil::parse_text("---@class C\n"
											 "C = {\n"
											 "  ---@type number\n"
											 "  a = 1,\n"
											 "  b = 2,\n"   // untyped — should NOT appear
											 "  c = 3,\n"   // untyped — should NOT appear
											 "  ---@type string\n"
											 "  d = \"x\",\n"
											 "}\n");
	ASSERT_EQ(out.size(), 1u);
	ASSERT_EQ(out[0].props.size(), 2u);
	EXPECT_EQ(out[0].props[0].name, "a");
	EXPECT_EQ(out[0].props[0].type_str, "number");
	EXPECT_EQ(out[0].props[1].name, "d");
	EXPECT_EQ(out[0].props[1].type_str, "string");
}

TEST_F(LuaComponentReflectionTest, ParserConsumesTypeAnnotationOncePerField) {
	// A single ---@type annotation applies to the NEXT field only, never carries over.
	auto out = ScriptLoadingUtil::parse_text("---@class C\n"
											 "C = {\n"
											 "  ---@type number\n"
											 "  a = 1,\n"
											 "  b = 2,\n"
											 "}\n");
	ASSERT_EQ(out.size(), 1u);
	ASSERT_EQ(out[0].props.size(), 1u);
	EXPECT_EQ(out[0].props[0].name, "a");
}

TEST_F(LuaComponentReflectionTest, SynthesisLaysOutSupportedScalarsAndDropsUnknown) {
	LuaClassTypeInfo cti;
	cti.set_classname("FakeComp");
	std::vector<ParseProperty> props = {
		{"hp", "number"},
		{"alive", "boolean"},
		{"name", "string"},
		{"weird", "SomeUnknownType"}, // dropped with a warning
	};
	cti.set_parsed_properties(std::move(props));
	cti.synthesize_lua_props_unchecked_for_test();

	auto* list = cti.get_lua_props_list();
	ASSERT_NE(list, nullptr);
	ASSERT_EQ(list->count, 3); // weird/SomeUnknownType skipped

	auto& storage = cti.get_lua_props_storage();
	EXPECT_EQ(storage[0].type, core_type_id::Float);
	EXPECT_EQ(storage[1].type, core_type_id::Bool);
	EXPECT_EQ(storage[2].type, core_type_id::StdString);

	for (auto& pi : storage) {
		EXPECT_TRUE(pi.flags & PROP_LUA_BACKED);
		EXPECT_TRUE(pi.can_edit());
		EXPECT_TRUE(pi.can_serialize());
	}

	// Layout: bool(1B) then float(align 4 → offset 4) then std::string(align alignof(string)).
	EXPECT_EQ(storage[0].offset, 0); // hp (float)
	EXPECT_EQ(storage[1].offset, 4); // alive (bool, after float)
	EXPECT_LE(storage[2].offset, cti.get_lua_field_shadow_size());
	EXPECT_GE(cti.get_lua_field_shadow_size(), 4u + 1u + sizeof(std::string));
}

TEST_F(LuaComponentReflectionTest, EmptyParsedPropsProducesNullList) {
	LuaClassTypeInfo cti;
	cti.set_classname("Empty");
	cti.set_parsed_properties({});
	cti.synthesize_lua_props_unchecked_for_test();
	EXPECT_EQ(cti.get_lua_props_list(), nullptr);
	EXPECT_EQ(cti.get_lua_field_shadow_size(), 0u);
}

TEST_F(LuaComponentReflectionTest, ResynthesizeReplacesPriorLayout) {
	// Hot-reload changes the field set: re-running synthesis must clear stale entries.
	LuaClassTypeInfo cti;
	cti.set_classname("Reload");
	cti.set_parsed_properties({{"a", "number"}, {"b", "number"}});
	cti.synthesize_lua_props_unchecked_for_test();
	ASSERT_NE(cti.get_lua_props_list(), nullptr);
	EXPECT_EQ(cti.get_lua_props_list()->count, 2);

	cti.set_parsed_properties({{"a", "number"}}); // b removed
	cti.synthesize_lua_props_unchecked_for_test();
	ASSERT_NE(cti.get_lua_props_list(), nullptr);
	EXPECT_EQ(cti.get_lua_props_list()->count, 1);
	EXPECT_STREQ(cti.get_lua_props_storage()[0].name, "a");
}
