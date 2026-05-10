#include <gtest/gtest.h>
// ClassBase.h must be included before ClassTypeInfo.h (via ScriptManager.h).
// ClassBase.h includes ClassTypeInfo.h at its bottom to complete the circular
// dependency. If ClassTypeInfo.h is processed first, ClassBase.h's include of
// ClassTypeInfo.h is blocked by the include guard, leaving ClassTypeInfo
// incomplete when ClassBase.h's templates are compiled.
#include "Framework/ClassBase.h"
#include "Scripting/ScriptManager.h"
#include "Testheader.h"

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

class ClassBaseRegistryTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); }

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
		return std::make_unique<ClassTypeInfo>(kName, &ClassBase::StaticType,
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
