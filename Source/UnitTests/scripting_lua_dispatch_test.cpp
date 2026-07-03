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
// Part 4: Lua virtual function dispatch end-to-end
//
// InterfaceClass (Testheader.h) is a scriptable C++ base with
// two virtual functions: get_value(string)→int and buzzer()→void.
// ScriptImpl_InterfaceClass (codegen) dispatches those calls to
// Lua when the instance table holds an override key.
//
// All tests share a single ClassBase init via SetUpTestSuite.
// Each test owns its own ScriptManager and registers/unregisters
// its Lua classes around each test.
//
// Cleanup order per TearDown:
//   1. Delete allocated C++ objects (frees their Lua tables while
//      the Lua state is still open).
//   2. free_table_registry_id() on each LuaClassTypeInfo to zero
//      out lua_table_id before lua_close() runs in ~ScriptManager.
//   3. unregister_class + post_changes_class_init.
//   4. delete ScriptManager (~ClassBase on LuaClassTypeInfos sees
//      lua_table_id == 0 and is therefore a no-op).
// ============================================================

class LuaScriptableClassTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); }

	void SetUp() override {
		sm = new ScriptManager();
		ASSERT(!ScriptManager::inst);
		ScriptManager::inst = sm;
		// init_class_info_for_script() calls init_this_class_type() for
		// every registered C++ class (sets lua_prototype_index_table) and
		// then set_class_type_global() for each (sets lua_table_id).
		// Both are required:
		//  - set_superclass() reads the parent class's lua_prototype_index_table.
		//  - create_class_table_for(luaInfo) reads ClassTypeInfo::StaticType's
		//    lua_prototype_index_table via luaInfo->get_type().
		ClassBase::init_class_info_for_script();
	}

	void TearDown() override {
		// 1. Delete allocated instances while the Lua state is still open.
		for (auto* obj : tracked_objs)
			delete obj;
		tracked_objs.clear();

		// 2. Unregister Lua classes and free their Lua table references
		//    before the ScriptManager destroys them.
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

		// 3. init_class_info_for_script() set lua_table_id on every C++
		//    ClassTypeInfo static.  Clear them all before lua_close() runs
		//    in ~ScriptManager; otherwise ~ClassBase() on those statics
		//    would call free_class_table() on an already-closed Lua state.
		{
			auto iter = ClassBase::get_subclasses(&ClassBase::StaticType);
			while (!iter.is_end()) {
				auto* info = const_cast<ClassTypeInfo*>(iter.get_type());
				if (info)
					info->free_table_registry_id();
				iter.next();
			}
		}

		// 4. lua_close() in ~ScriptManager is now safe: all lua_table_ids
		//    are 0, so ~ClassBase() on any remaining object is a no-op.
		delete sm;
		sm = nullptr;
		ScriptManager::inst = nullptr;
	}

	// Execute Lua source text and flush class registration.
	void load_lua(const std::string& src, std::initializer_list<const char*> class_names) {
		sm->reload_from_content(src, "test");
		sm->check_for_reload();
		for (auto* n : class_names)
			lua_class_names.push_back(n);
	}

	// Allocate a Lua class instance, tracking it for cleanup.
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

// After check_for_reload the class name should appear in the registry.
TEST_F(LuaScriptableClassTest, ClassRegisteredAfterReload) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n",
			 {"LuaTestImpl"});

	EXPECT_NE(ClassBase::find_class("LuaTestImpl"), nullptr);
}

// allocate_class must return an object that is an InterfaceClass.
TEST_F(LuaScriptableClassTest, AllocatedInstanceIsInterfaceClass) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	EXPECT_NE(obj->cast_to<InterfaceClass>(), nullptr);
}

// A Lua override of get_value should be called instead of C++ base.
TEST_F(LuaScriptableClassTest, LuaOverride_GetValueReturnsCustom) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:get_value(str)\n"
			 "    return 42\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->get_value("anything"), 42);
}

// With no Lua override, the C++ base implementation returns 0.
TEST_F(LuaScriptableClassTest, LuaNoOverride_GetValueFallsBackToCpp) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->get_value("x"), 0);
}

// Lua buzzer override can write to Lua globals; verify side-effect.
TEST_F(LuaScriptableClassTest, LuaOverride_BuzzerWithSideEffect) {
	load_lua("buzzer_call_count = 0\n"
			 "---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    buzzer_call_count = buzzer_call_count + 1\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	iface->buzzer();
	iface->buzzer();

	lua_State* L = sm->get_lua_state();
	lua_getglobal(L, "buzzer_call_count");
	EXPECT_EQ((int)lua_tointeger(L, -1), 2);
	lua_pop(L, 1);
}

// The string argument passed to get_value reaches the Lua override.
TEST_F(LuaScriptableClassTest, LuaOverride_GetValueUsesStringArg) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:get_value(str)\n"
			 "    return #str\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->get_value("hello"), 5);
	EXPECT_EQ(iface->get_value(""), 0);
}

// self: inside a Lua override refers to the C++ object table;
// calling a C++ method via self mutates the C++ side.
TEST_F(LuaScriptableClassTest, LuaOverride_BuzzerCallsCppViaSelf) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    self:set_var(99)\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	iface->buzzer();
	EXPECT_EQ(iface->variable, 99);
}

// Two instances of the same Lua class maintain independent C++ state.
TEST_F(LuaScriptableClassTest, MultipleInstances_AreIndependent) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    self:set_var(self:self_func() + 1)\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj1 = alloc_obj("LuaTestImpl");
	ClassBase* obj2 = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj1, nullptr);
	ASSERT_NE(obj2, nullptr);
	InterfaceClass* iface1 = obj1->cast_to<InterfaceClass>();
	InterfaceClass* iface2 = obj2->cast_to<InterfaceClass>();
	ASSERT_NE(iface1, nullptr);
	ASSERT_NE(iface2, nullptr);

	iface1->buzzer(); // variable: 0 → 1
	iface1->buzzer(); // variable: 1 → 2
	iface2->buzzer(); // variable: 0 → 1  (independent)

	EXPECT_EQ(iface1->variable, 2);
	EXPECT_EQ(iface2->variable, 1);
}

// Lua table properties defined in the class body are shallow-copied
// into each instance table and accessible via self.
TEST_F(LuaScriptableClassTest, InstanceProperty_CopiedFromTemplate) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {\n"
			 "    init_val = 77\n"
			 "}\n"
			 "function LuaTestImpl:get_value(str)\n"
			 "    return self.init_val\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);
	EXPECT_EQ(iface->get_value(""), 77);
}

// A Lua runtime error in an override must surface as LuaRuntimeError.
TEST_F(LuaScriptableClassTest, LuaRuntimeError_InOverrideThrows) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:get_value(str)\n"
			 "    error('intentional test error')\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);
	EXPECT_THROW(iface->get_value("x"), LuaRuntimeError);
}

// Both virtual functions dispatch independently to their Lua overrides.
TEST_F(LuaScriptableClassTest, BothVirtuals_DispatchedIndependently) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:get_value(str)\n"
			 "    return 7\n"
			 "end\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    self:set_var(3)\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	EXPECT_EQ(iface->get_value("test"), 7);
	iface->buzzer();
	EXPECT_EQ(iface->variable, 3);
}

// Load a richer override from a fixture file; verifies argument use,
// self-property mutation, and repeated calls across both virtuals.
TEST_F(LuaScriptableClassTest, FixtureFile_OverridesAndSelfAccess) {
	auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "TestFiles" / "Scripting" /
				"test_lua_scriptable.lua";
	std::ifstream f(path);
	ASSERT_TRUE(f.is_open()) << "Fixture missing: " << path;
	std::string src(std::istreambuf_iterator<char>(f), {});

	load_lua(src, {"LuaFixtureImpl"});

	ClassBase* obj = alloc_obj("LuaFixtureImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	// get_value returns 7 + #str
	EXPECT_EQ(iface->get_value("abc"), 10); // 7 + 3
	EXPECT_EQ(iface->get_value(""), 7);		// 7 + 0

	// buzzer: increments call_count, then set_var(call_count * 10)
	iface->buzzer();
	EXPECT_EQ(iface->variable, 10); // call_count=1
	iface->buzzer();
	EXPECT_EQ(iface->variable, 20); // call_count=2
}

// ============================================================
// StringName Lua/C++ boundary: exercises get_stringname_from_lua /
// push_stringname_to_lua (ScriptFunctionCodegen.cpp) through the real
// codegen'd bindings on InterfaceClass::get_name_field/set_name_field.
// ============================================================

// C++ -> Lua: a StringName set from C++ must be readable from a Lua override,
// comparing equal to a hash the Lua side interned itself via LuaSystem.name(...).
TEST_F(LuaScriptableClassTest, StringName_CppToLua_ComparesEqualByHash) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    local n = self:get_name_field()\n"
			 "    if n == LuaSystem.name('foo_bar_baz') then\n"
			 "        self:set_var(1)\n"
			 "    else\n"
			 "        self:set_var(0)\n"
			 "    end\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	iface->set_name_field(StringName("foo_bar_baz"));
	iface->buzzer();
	EXPECT_EQ(iface->variable, 1);
}

// Lua -> C++: a plain Lua string literal passed into a StringName parameter must intern
// to the same StringName as the equivalent C++-constructed one.
TEST_F(LuaScriptableClassTest, StringName_LuaStringLiteral_InternsToSameName) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    self:set_name_field('hello_from_lua')\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	iface->buzzer();
	EXPECT_TRUE(iface->name_field_is("hello_from_lua"));
}

// Lua -> C++: an already-interned hash (from LuaSystem.name) must also be accepted directly,
// with no re-hashing (dual string/integer accept in get_stringname_from_lua).
TEST_F(LuaScriptableClassTest, StringName_LuaPrehashedInteger_IsAccepted) {
	load_lua("---@class LuaTestImpl : InterfaceClass\n"
			 "LuaTestImpl = {}\n"
			 "local N_CACHED = LuaSystem.name('cached_name')\n"
			 "function LuaTestImpl:buzzer()\n"
			 "    self:set_name_field(N_CACHED)\n"
			 "end\n",
			 {"LuaTestImpl"});

	ClassBase* obj = alloc_obj("LuaTestImpl");
	ASSERT_NE(obj, nullptr);
	InterfaceClass* iface = obj->cast_to<InterfaceClass>();
	ASSERT_NE(iface, nullptr);

	iface->buzzer();
	EXPECT_TRUE(iface->name_field_is("cached_name"));
}
