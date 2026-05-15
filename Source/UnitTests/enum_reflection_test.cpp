#include <gtest/gtest.h>
#include "Framework/EnumDefReflection.h"

// EnumTypeInfo's ctor registers itself in the global EnumRegistry by pointer, so the
// instance must outlive every test. A single static fixture instance does both — gives
// the tests a stable target without risking a dangling pointer in the global registry.
namespace {
const EnumIntPair kPairs[] = {
	EnumIntPair("Red",   "Red",   1),
	EnumIntPair("Green", "Green", 2),
	EnumIntPair("Blue",  "Blue",  3),
};
static EnumTypeInfo kColorEnum("EnumReflectionTest_Color", kPairs, 3);
}

TEST(EnumReflection, FindForValue_Hit) {
	auto* p = kColorEnum.find_for_value(2);
	ASSERT_NE(p, nullptr);
	EXPECT_STREQ(p->name, "Green");
}

TEST(EnumReflection, FindForValue_Miss) {
	EXPECT_EQ(kColorEnum.find_for_value(99), nullptr);
}

TEST(EnumReflection, FindForName_Hit) {
	auto* p = kColorEnum.find_for_name("Blue");
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->value, 3);
}

TEST(EnumReflection, FindForName_Miss) {
	EXPECT_EQ(kColorEnum.find_for_name("Mauve"), nullptr);
	EXPECT_EQ(kColorEnum.find_for_name(nullptr), nullptr);
}
