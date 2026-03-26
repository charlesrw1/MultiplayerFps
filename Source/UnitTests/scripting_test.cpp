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
    testInfo = make_test_class();
    ClassBase::register_class(testInfo.get());
    ClassBase::post_changes_class_init();
    ClassBase::unregister_class(testInfo.get());
    ClassBase::post_changes_class_init();
    testInfo.reset();

    EXPECT_NE(ClassBase::find_class("ClassBase"), nullptr);
    EXPECT_TRUE(ClassBase::StaticType.is_a(ClassBase::StaticType));
}
