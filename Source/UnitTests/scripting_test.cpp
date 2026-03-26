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

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ============================================================
// Part 1: ScriptLoadingUtil::parse_text() — pure unit tests
//
// parse_text() is a pure function: it takes Lua source text and
// returns the class/property structure declared via annotation
// comments. No ClassBase or FileSys involvement.
// ============================================================

TEST(ParseText, EmptyString) {
    EXPECT_TRUE(ScriptLoadingUtil::parse_text("").empty());
}

TEST(ParseText, PlainLuaNoAnnotations) {
    EXPECT_TRUE(ScriptLoadingUtil::parse_text("local x = 5\nfunction foo() end\n").empty());
}

TEST(ParseText, AnnotationWithoutTableBodyNotEmitted) {
    // Class annotation with no matching table body: inClass never becomes true,
    // so the pending class is never pushed to output.
    EXPECT_TRUE(ScriptLoadingUtil::parse_text("---@class Orphan\n").empty());
}

TEST(ParseText, SimpleClassWithEmptyBody) {
    auto r = ScriptLoadingUtil::parse_text("---@class Foo\nFoo = {\n}\n");
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "Foo");
    EXPECT_TRUE(r[0].inherited.empty());
    EXPECT_TRUE(r[0].props.empty());
}

TEST(ParseText, ClassWithSingleParent) {
    auto r = ScriptLoadingUtil::parse_text("---@class Child : Parent\nChild = {\n}\n");
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "Child");
    ASSERT_EQ(r[0].inherited.size(), 1u);
    EXPECT_EQ(r[0].inherited[0], "Parent");
}

TEST(ParseText, ClassWithMultipleParents) {
    auto r = ScriptLoadingUtil::parse_text("---@class Child : A, B\nChild = {\n}\n");
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].inherited.size(), 2u);
    EXPECT_EQ(r[0].inherited[0], "A");
    EXPECT_EQ(r[0].inherited[1], "B");
}

TEST(ParseText, MultipleClassesInFile) {
    const char* src =
        "---@class Foo\nFoo = {\n}\n"
        "---@class Bar : Foo\nBar = {\n}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0].name, "Foo");
    EXPECT_EQ(r[1].name, "Bar");
    ASSERT_EQ(r[1].inherited.size(), 1u);
    EXPECT_EQ(r[1].inherited[0], "Foo");
}

TEST(ParseText, PropertyWithTypeAnnotation) {
    const char* src =
        "---@class Foo\n"
        "Foo = {\n"
        "---@type number\n"
        "health = 0,\n"
        "}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].props.size(), 1u);
    EXPECT_EQ(r[0].props[0].name, "health");
    EXPECT_EQ(r[0].props[0].type_str, "number");
}

TEST(ParseText, PropertyWithoutTypeAnnotation) {
    const char* src =
        "---@class Foo\n"
        "Foo = {\n"
        "speed = 10,\n"
        "}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].props.size(), 1u);
    EXPECT_EQ(r[0].props[0].name, "speed");
    EXPECT_TRUE(r[0].props[0].type_str.empty());
}

TEST(ParseText, MultipleProperties) {
    const char* src =
        "---@class Foo\n"
        "Foo = {\n"
        "---@type number\n"
        "health = 100,\n"
        "---@type string\n"
        "name = \"\",\n"
        "}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].props.size(), 2u);
    EXPECT_EQ(r[0].props[0].name, "health");
    EXPECT_EQ(r[0].props[0].type_str, "number");
    EXPECT_EQ(r[0].props[1].name, "name");
    EXPECT_EQ(r[0].props[1].type_str, "string");
}

TEST(ParseText, OneLineClassBody) {
    auto r = ScriptLoadingUtil::parse_text("---@class Foo\nFoo = { }\n");
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "Foo");
    EXPECT_TRUE(r[0].props.empty());
}

TEST(ParseText, IndentedTableStartNotRecognized) {
    // Table variable must start at column 0 (starts_with check in parser).
    // An indented table declaration doesn't trigger inClass=true.
    auto r = ScriptLoadingUtil::parse_text("---@class Foo\n  Foo = {\n}\n");
    EXPECT_TRUE(r.empty());
}

TEST(ParseText, FirstAnnotationLostWhenSecondArrivesWithoutBody) {
    // When a second ---@class annotation is seen while inClass is false,
    // the first pending class (no body) is silently discarded.
    const char* src =
        "---@class Lost\n"
        "---@class Found\n"
        "Found = {\n}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "Found");
}

TEST(ParseText, FileEndsInsideClassStillEmitted) {
    // No closing brace: inClass=true at EOF means the last class is still emitted.
    const char* src =
        "---@class Unclosed\n"
        "Unclosed = {\n"
        "value = 1,\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "Unclosed");
    ASSERT_EQ(r[0].props.size(), 1u);
}

TEST(ParseText, TypeAnnotationNotCarriedAcrossProperties) {
    // A @type annotation applies only to the immediately following property.
    const char* src =
        "---@class Foo\n"
        "Foo = {\n"
        "---@type number\n"
        "a = 0,\n"
        "b = 0,\n"
        "}\n";
    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].props.size(), 2u);
    EXPECT_EQ(r[0].props[0].type_str, "number");
    EXPECT_TRUE(r[0].props[1].type_str.empty());
}

TEST(ParseText, FixtureFileTestClassNoInherit) {
    // Parse the fixture file that declares a class with no inheritance.
    auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                / "TestFiles" / "Scripting" / "test_class_no_inherit.lua";
    std::ifstream f(path);
    ASSERT_TRUE(f.is_open()) << "Fixture file missing: " << path;
    std::string src(std::istreambuf_iterator<char>(f), {});

    auto r = ScriptLoadingUtil::parse_text(src);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].name, "TestNoInherit");
    EXPECT_TRUE(r[0].inherited.empty());
    ASSERT_EQ(r[0].props.size(), 2u);
    EXPECT_EQ(r[0].props[0].name, "value");
    EXPECT_EQ(r[0].props[0].type_str, "number");
    EXPECT_EQ(r[0].props[1].name, "label");
    EXPECT_EQ(r[0].props[1].type_str, "string");
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

class ScriptManagerTest : public ::testing::Test {
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
    sm->reload_from_content(
        "---@class MyLuaClass\n"
        "MyLuaClass = {\n"
        "  value = 10,\n"
        "}\n",
        "test");
    EXPECT_EQ(lua_gettop(sm->get_lua_state()), 0);
}

TEST_F(ScriptManagerTest, LoadFromFixtureFile) {
    auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                / "TestFiles" / "Scripting" / "test_basic.lua";
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

// ============================================================
// Part 3: ClassBase registry reset
//
// unregister_class() removes from the name->ClassTypeInfo map but
// does NOT touch the id vector. post_changes_class_init() rebuilds
// the id vector from scratch, so the safe reset pattern is:
//   ClassBase::unregister_class(info);
//   ClassBase::post_changes_class_init();
//
// find_class() asserts registry.initialized, set only by
// init_classes_startup(). We call it once in SetUpTestSuite.
// init_classes_startup() allocates default objects for C++ classes
// and serializes them — if any crash, those classes need engine
// setup and this fixture must move to the integration suite.
//
// ClassTypeInfo with is_lua_obj=true skips auto-registration,
// letting us manually control register/unregister in tests.
// ============================================================

class ClassBaseRegistryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ClassBase::init_classes_startup();
    }

    void TearDown() override {
        // Always clean up the temporary class if the test left it registered.
        if (ClassBase::does_class_exist(kName)) {
            ClassBase::unregister_class(testInfo.get());
            ClassBase::post_changes_class_init();
        }
        testInfo.reset();
    }

    // Make a ClassTypeInfo that won't auto-register (is_lua_obj=true).
    // Caller owns it and must register/unregister manually.
    std::unique_ptr<ClassTypeInfo> make_test_class() {
        return std::make_unique<ClassTypeInfo>(
            kName, &ClassBase::StaticType,
            /*get_props=*/nullptr, /*alloc=*/nullptr,
            /*create_default_obj=*/false,
            /*lua_funcs=*/nullptr, /*lua_func_count=*/0,
            /*scriptable_alloc=*/nullptr, /*is_lua_obj=*/true);
    }

    static constexpr const char* kName = "__unit_test_classreg__";
    std::unique_ptr<ClassTypeInfo> testInfo;
};

TEST_F(ClassBaseRegistryTest, BuiltinClassFoundAfterInit) {
    // Sanity check: init_classes_startup ran and ClassBase itself is findable.
    EXPECT_NE(ClassBase::find_class("ClassBase"), nullptr);
}

TEST_F(ClassBaseRegistryTest, RegisteredClassIsFoundByName) {
    testInfo = make_test_class();
    ClassBase::register_class(testInfo.get());
    ClassBase::post_changes_class_init();

    EXPECT_EQ(ClassBase::find_class(kName), testInfo.get());
}

TEST_F(ClassBaseRegistryTest, RegisteredClassIsInClassBaseSubtree) {
    // After post_changes_class_init rebuilds the ID tree, the new class
    // should report as a subclass of ClassBase via is_a().
    testInfo = make_test_class();
    ClassBase::register_class(testInfo.get());
    ClassBase::post_changes_class_init();

    EXPECT_TRUE(testInfo->is_a(ClassBase::StaticType));
}

TEST_F(ClassBaseRegistryTest, UnregisteredClassIsNoLongerFound) {
    testInfo = make_test_class();
    ClassBase::register_class(testInfo.get());
    ClassBase::post_changes_class_init();
    ASSERT_NE(ClassBase::find_class(kName), nullptr);

    ClassBase::unregister_class(testInfo.get());
    ClassBase::post_changes_class_init();

    EXPECT_EQ(ClassBase::find_class(kName), nullptr);
}

TEST_F(ClassBaseRegistryTest, ExistingClassesUnaffectedByAddRemove) {
    // Registering and unregistering a temp class should leave ClassBase intact.

    auto classIter = ClassBase::get_subclasses(&ClassBase::StaticType);
	std::vector<std::string> all_classes;
	while (classIter.is_end()) {
		ASSERT(classIter.get_type());
		all_classes.push_back(classIter.get_type()->classname);
		classIter = classIter.next();
    }

    testInfo = make_test_class();
    ClassBase::register_class(testInfo.get());
    ClassBase::post_changes_class_init();
    ClassBase::unregister_class(testInfo.get());
    ClassBase::post_changes_class_init();
    testInfo.reset();

    for (auto& s : all_classes)
        EXPECT_NE(ClassBase::find_class(s.c_str()), nullptr);
    EXPECT_TRUE(ClassBase::StaticType.is_a(ClassBase::StaticType));
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

class LuaScriptableClassTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ClassBase::init_classes_startup();
    }

    void SetUp() override {
        sm = new ScriptManager();
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
    void load_lua(const std::string& src,
                  std::initializer_list<const char*> class_names) {
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
        "LuaTestImpl = {}\n",
        {"LuaTestImpl"});

    EXPECT_NE(ClassBase::find_class("LuaTestImpl"), nullptr);
}

// allocate_class must return an object that is an InterfaceClass.
TEST_F(LuaScriptableClassTest, AllocatedInstanceIsInterfaceClass) {
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
        "LuaTestImpl = {}\n",
        {"LuaTestImpl"});

    ClassBase* obj = alloc_obj("LuaTestImpl");
    ASSERT_NE(obj, nullptr);
    EXPECT_NE(obj->cast_to<InterfaceClass>(), nullptr);
}

// A Lua override of get_value should be called instead of C++ base.
TEST_F(LuaScriptableClassTest, LuaOverride_GetValueReturnsCustom) {
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "buzzer_call_count = 0\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    load_lua(
        "---@class LuaTestImpl : InterfaceClass\n"
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
    auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                / "TestFiles" / "Scripting" / "test_lua_scriptable.lua";
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
    EXPECT_EQ(iface->get_value(""), 7);     // 7 + 0

    // buzzer: increments call_count, then set_var(call_count * 10)
    iface->buzzer();
    EXPECT_EQ(iface->variable, 10); // call_count=1
    iface->buzzer();
    EXPECT_EQ(iface->variable, 20); // call_count=2
}
