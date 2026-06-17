#include <gtest/gtest.h>
#include <string>
using std::string;
#include "Framework/ClassBase.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Scripting/ScriptManager.h"
#include "Testheader.h"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ============================================================
// Pure C++ interface tests (no Lua state needed)
// ============================================================

TEST(InterfaceTest, InterfaceTypeInfoRegistration) {
	auto* info = InterfaceTypeInfo::find_interface("ITestInterface");
	ASSERT_NE(info, nullptr);
	EXPECT_STREQ(info->name, "ITestInterface");
	EXPECT_GE(info->id, 0);

	auto* info2 = InterfaceTypeInfo::find_interface("ISecondInterface");
	ASSERT_NE(info2, nullptr);
	EXPECT_NE(info->id, info2->id);

	auto* by_id = InterfaceTypeInfo::find_interface(info->id);
	EXPECT_EQ(by_id, info);
}

TEST(InterfaceTest, ClassHasInterface) {
	auto obj = std::make_unique<TestWithInterface>();
	EXPECT_TRUE(obj->has_interface<ITestInterface>());
	EXPECT_FALSE(obj->has_interface<ISecondInterface>());
}

TEST(InterfaceTest, ClassHasTwoInterfaces) {
	auto obj = std::make_unique<TestWithTwoInterfaces>();
	EXPECT_TRUE(obj->has_interface<ITestInterface>());
	EXPECT_TRUE(obj->has_interface<ISecondInterface>());
}

TEST(InterfaceTest, CastInterface) {
	auto obj = std::make_unique<TestWithInterface>();
	auto* iface = obj->cast_interface<ITestInterface>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->interface_value(), 100);

	auto* no_iface = obj->cast_interface<ISecondInterface>();
	EXPECT_EQ(no_iface, nullptr);
}

TEST(InterfaceTest, CastTwoInterfaces) {
	auto obj = std::make_unique<TestWithTwoInterfaces>();
	auto* iface1 = obj->cast_interface<ITestInterface>();
	ASSERT_NE(iface1, nullptr);
	EXPECT_EQ(iface1->interface_value(), 200);

	auto* iface2 = obj->cast_interface<ISecondInterface>();
	ASSERT_NE(iface2, nullptr);
	EXPECT_EQ(iface2->second_name(), "two_ifaces");
}

TEST(InterfaceTest, HasInterfaceById) {
	auto obj = std::make_unique<TestWithInterface>();
	auto* info = InterfaceTypeInfo::find_interface("ITestInterface");
	ASSERT_NE(info, nullptr);
	EXPECT_TRUE(obj->has_interface_by_id(info->id));
	EXPECT_FALSE(obj->has_interface_by_id(999));
}

TEST(InterfaceTest, InterfacePropagation) {
	EXPECT_TRUE(TestWithInterface::StaticType.has_interface(ITestInterface::StaticInterfaceType.id));
	EXPECT_TRUE(TestWithTwoInterfaces::StaticType.has_interface(ITestInterface::StaticInterfaceType.id));
	EXPECT_TRUE(TestWithTwoInterfaces::StaticType.has_interface(ISecondInterface::StaticInterfaceType.id));
}

// ============================================================
// Lua integration tests — interface method dispatch + queries
//
// Uses the same fixture pattern as LuaScriptableClassTest.
// TestWithInterface is scriptable, so Lua subclasses can
// override interface_value() and interface_action().
// ============================================================

class LuaInterfaceTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); }

	void SetUp() override {
		sm = new ScriptManager();
		ASSERT(!ScriptManager::inst);
		ScriptManager::inst = sm;
		ClassBase::init_class_info_for_script();
	}

	void TearDown() override {
		for (auto* obj : tracked_objs)
			delete obj;
		tracked_objs.clear();

		for (const auto& name : lua_class_names) {
			auto* info = const_cast<ClassTypeInfo*>(ClassBase::find_class(name.c_str()));
			if (info) {
				info->free_table_registry_id();
				ClassBase::unregister_class(info);
			}
		}
		if (!lua_class_names.empty())
			ClassBase::post_changes_class_init();
		lua_class_names.clear();

		{
			auto iter = ClassBase::get_subclasses(&ClassBase::StaticType);
			while (!iter.is_end()) {
				auto* info = const_cast<ClassTypeInfo*>(iter.get_type());
				if (info)
					info->free_table_registry_id();
				iter.next();
			}
		}

		delete sm;
		sm = nullptr;
		ScriptManager::inst = nullptr;
	}

	void load_lua(const std::string& src, std::initializer_list<const char*> class_names) {
		sm->reload_from_content(src, "test");
		sm->check_for_reload();
		for (auto* n : class_names)
			lua_class_names.push_back(n);
	}

	ClassBase* alloc_obj(const char* name) {
		ClassBase* obj = sm->allocate_class(name);
		if (obj)
			tracked_objs.push_back(obj);
		return obj;
	}

	ScriptManager* sm = nullptr;
	std::vector<ClassBase*> tracked_objs;
	std::vector<std::string> lua_class_names;
};

// Lua can call interface_value() on a C++ TestWithInterface object
TEST_F(LuaInterfaceTest, LuaCallsInterfaceMethodOnCppObject) {
	auto obj = std::make_unique<TestWithInterface>();
	lua_State* L = sm->get_lua_state();
	// Push the C++ object as a Lua userdata via its table
	int table_id = obj->get_table_registry_id();
	lua_rawgeti(L, LUA_REGISTRYINDEX, table_id);
	lua_setglobal(L, "test_obj");

	// Call interface_value() from Lua and capture the result
	const char* code = "test_result = test_obj:interface_value()";
	ASSERT_EQ(luaL_dostring(L, code), LUA_OK) << lua_tostring(L, -1);

	lua_getglobal(L, "test_result");
	EXPECT_EQ((int)lua_tointeger(L, -1), 100);
	lua_pop(L, 1);

	obj->free_table_registry_id();
}

// Lua can call has_interface_by_id() with the pushed ITestInterfaceId global
TEST_F(LuaInterfaceTest, LuaHasInterfaceByIdWorks) {
	auto obj = std::make_unique<TestWithInterface>();
	lua_State* L = sm->get_lua_state();
	int table_id = obj->get_table_registry_id();
	lua_rawgeti(L, LUA_REGISTRYINDEX, table_id);
	lua_setglobal(L, "test_obj");

	// ITestInterfaceId is pushed by init_class_info_for_script
	const char* code =
		"has_it = test_obj:has_interface_by_id(ITestInterfaceId)\n"
		"no_it = test_obj:has_interface_by_id(ISecondInterfaceId)\n";
	ASSERT_EQ(luaL_dostring(L, code), LUA_OK) << lua_tostring(L, -1);

	lua_getglobal(L, "has_it");
	EXPECT_TRUE(lua_toboolean(L, -1));
	lua_pop(L, 1);

	lua_getglobal(L, "no_it");
	EXPECT_FALSE(lua_toboolean(L, -1));
	lua_pop(L, 1);

	obj->free_table_registry_id();
}

// Lua subclass of TestWithInterface can override interface_value()
TEST_F(LuaInterfaceTest, LuaOverridesInterfaceVirtual) {
	load_lua("---@class LuaIfaceImpl : TestWithInterface\n"
			 "LuaIfaceImpl = {}\n"
			 "function LuaIfaceImpl:interface_value()\n"
			 "    return 999\n"
			 "end\n",
			 {"LuaIfaceImpl"});

	ClassBase* obj = alloc_obj("LuaIfaceImpl");
	ASSERT_NE(obj, nullptr);
	auto* iface = obj->cast_interface<ITestInterface>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->interface_value(), 999);
}

// Without Lua override, interface virtual falls back to C++ base
TEST_F(LuaInterfaceTest, LuaNoOverrideFallsBackToCpp) {
	load_lua("---@class LuaIfaceImpl : TestWithInterface\n"
			 "LuaIfaceImpl = {}\n",
			 {"LuaIfaceImpl"});

	ClassBase* obj = alloc_obj("LuaIfaceImpl");
	ASSERT_NE(obj, nullptr);
	auto* iface = obj->cast_interface<ITestInterface>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->interface_value(), 100);
}

// Lua subclass inherits the interface — has_interface should be true
TEST_F(LuaInterfaceTest, LuaSubclassInheritsInterface) {
	load_lua("---@class LuaIfaceImpl : TestWithInterface\n"
			 "LuaIfaceImpl = {}\n",
			 {"LuaIfaceImpl"});

	ClassBase* obj = alloc_obj("LuaIfaceImpl");
	ASSERT_NE(obj, nullptr);
	EXPECT_TRUE(obj->has_interface<ITestInterface>());
	EXPECT_FALSE(obj->has_interface<ISecondInterface>());
}

// Lua code can call interface methods on a Lua-allocated object
TEST_F(LuaInterfaceTest, LuaCallsInterfaceMethodOnLuaObject) {
	load_lua("---@class LuaIfaceImpl : TestWithInterface\n"
			 "LuaIfaceImpl = {}\n"
			 "function LuaIfaceImpl:interface_value()\n"
			 "    return 555\n"
			 "end\n",
			 {"LuaIfaceImpl"});

	ClassBase* obj = alloc_obj("LuaIfaceImpl");
	ASSERT_NE(obj, nullptr);

	lua_State* L = sm->get_lua_state();
	lua_rawgeti(L, LUA_REGISTRYINDEX, obj->get_table_registry_id());
	lua_setglobal(L, "lua_obj");

	const char* code = "lua_result = lua_obj:interface_value()";
	ASSERT_EQ(luaL_dostring(L, code), LUA_OK) << lua_tostring(L, -1);

	lua_getglobal(L, "lua_result");
	EXPECT_EQ((int)lua_tointeger(L, -1), 555);
	lua_pop(L, 1);
}
