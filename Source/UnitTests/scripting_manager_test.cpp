#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
// ClassBase.h must be included before ClassTypeInfo.h (via ScriptManager.h).
// ClassBase.h includes ClassTypeInfo.h at its bottom to complete the circular
// dependency. If ClassTypeInfo.h is processed first, ClassBase.h's include of
// ClassTypeInfo.h is blocked by the include guard, leaving ClassTypeInfo
// incomplete when ClassBase.h's templates are compiled.
#include "Framework/ClassBase.h"
#include "Scripting/ScriptManager.h"
#include "Testheader.h"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ============================================================
// Part 2: ScriptManager — Lua execution integration tests
//
// These tests verify Lua loading and execution. They do NOT test
// Lua class registration (LuaClassTypeInfo paths), because
// set_superclass() calls ClassBase::find_class() which asserts
// registry.initialized — only true after init_classes_startup().
// init_classes_startup() creates default objects for all C++
// classes and serializes them, which requires the full engine.
//
// Tests use reload_from_content() directly to avoid filesystem
// dependencies. reload_one_file() is a thin wrapper around it.
// ============================================================

class ScriptManagerTest : public ::testing::Test
{
protected:
	void SetUp() override {
		sm = new ScriptManager();
		ASSERT(!ScriptManager::inst);
		ScriptManager::inst = sm;
	}
	void TearDown() override {
		delete sm;
		sm = nullptr;
		ScriptManager::inst = nullptr;
	}
	ScriptManager* sm = nullptr;
};

TEST_F(ScriptManagerTest, ConstructAndDestructSafely) {
	EXPECT_NE(sm->get_lua_state(), nullptr);
}

TEST_F(ScriptManagerTest, FreshStateHasEmptyStack) {
	EXPECT_EQ(lua_gettop(sm->get_lua_state()), 0);
}

TEST_F(ScriptManagerTest, ExecuteValidLuaSetsGlobal) {
	sm->reload_from_content("test_val = 99", "test");
	auto* L = sm->get_lua_state();
	lua_getglobal(L, "test_val");
	ASSERT_TRUE(lua_isnumber(L, -1));
	EXPECT_EQ((int)lua_tointeger(L, -1), 99);
	lua_pop(L, 1);
}

TEST_F(ScriptManagerTest, DefinedFunctionIsCallable) {
	sm->reload_from_content("function add(a, b) return a + b end", "test");
	auto* L = sm->get_lua_state();
	lua_getglobal(L, "add");
	lua_pushinteger(L, 3);
	lua_pushinteger(L, 7);
	ASSERT_EQ(lua_pcall(L, 2, 1, 0), LUA_OK);
	EXPECT_EQ((int)lua_tointeger(L, -1), 10);
	lua_pop(L, 1);
}

TEST_F(ScriptManagerTest, SyntaxErrorLeavesCleanStack) {
	sm->reload_from_content("@@@ invalid lua @@@", "test");
	EXPECT_EQ(lua_gettop(sm->get_lua_state()), 0);
}

TEST_F(ScriptManagerTest, RuntimeErrorLeavesCleanStack) {
	sm->reload_from_content("error('deliberate test error')", "test");
	EXPECT_EQ(lua_gettop(sm->get_lua_state()), 0);
}

TEST_F(ScriptManagerTest, MultipleReloadsAccumulateGlobals) {
	sm->reload_from_content("a = 1", "chunk_a");
	sm->reload_from_content("b = 2", "chunk_b");
	auto* L = sm->get_lua_state();
	lua_getglobal(L, "a");
	lua_getglobal(L, "b");
	EXPECT_EQ((int)lua_tointeger(L, -2), 1);
	EXPECT_EQ((int)lua_tointeger(L, -1), 2);
	lua_pop(L, 2);
}

TEST_F(ScriptManagerTest, ClassAnnotationNoInheritanceDoesNotCrash) {
	// parse_text finds a class but inherited is empty, so set_superclass
	// (and find_class) is never called. Safe without ClassBase init.
	sm->reload_from_content("---@class MyLuaClass\n"
							"MyLuaClass = {\n"
							"  value = 10,\n"
							"}\n",
							"test");
	EXPECT_EQ(lua_gettop(sm->get_lua_state()), 0);
}

TEST_F(ScriptManagerTest, LoadFromFixtureFile) {
	auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "TestFiles" / "Scripting" /
				"test_basic.lua";
	std::ifstream f(path);
	ASSERT_TRUE(f.is_open()) << "Fixture file missing: " << path;
	std::string src(std::istreambuf_iterator<char>(f), {});

	sm->reload_from_content(src, "test_basic.lua");

	auto* L = sm->get_lua_state();
	lua_getglobal(L, "test_fixture_loaded");
	ASSERT_TRUE(lua_isboolean(L, -1)) << "test_fixture_loaded was not set";
	EXPECT_TRUE(lua_toboolean(L, -1));
	lua_pop(L, 1);
}
