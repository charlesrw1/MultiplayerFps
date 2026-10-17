#include <gtest/gtest.h>
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Testheader.h"

// PropertyInfo values (PropertyAccess) over C++ fields. LuaSharp fields: luasharp_engine_test.cpp.

namespace {
const PropertyInfo& prop(std::string_view name) {
	const PropertyInfoList* props = PropAccessTestThing::StaticType.props;
	EXPECT_NE(props, nullptr);
	for (int i = 0; i < props->count; i++)
		if (props->list[i].name == name)
			return props->list[i];
	ADD_FAILURE() << "no property " << name;
	static PropertyInfo none;
	return none;
}
} // namespace

class PropertyAccessTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); }
	PropAccessTestThing thing;
};

TEST_F(PropertyAccessTest, ClassTypeOwnsItsProperties) {
	EXPECT_EQ(prop("speed").owner, &PropAccessTestThing::StaticType);
	EXPECT_TRUE(prop("speed").has_value());
}

TEST_F(PropertyAccessTest, NumbersUseDomainTypes) {
	// A float field is a double value; an int32 is an int64 value with its sign.
	EXPECT_TRUE(std::holds_alternative<double>(prop("speed").get_value(&thing)));
	EXPECT_DOUBLE_EQ(prop("speed").get<double>(&thing), 1.5);
	EXPECT_EQ(prop("signed_count").get<int64_t>(&thing), -3);
	EXPECT_EQ(prop("mask").get<int>(&thing), 0xF0);

	prop("speed").set(&thing, 2.25);
	prop("signed_count").set(&thing, -40);
	prop("mask").set(&thing, 0x0F);
	prop("enabled").set(&thing, false);
	EXPECT_FLOAT_EQ(thing.speed, 2.25f);
	EXPECT_EQ(thing.signed_count, -40);
	EXPECT_EQ(thing.mask, 0x0F);
	EXPECT_FALSE(thing.enabled);
}

TEST_F(PropertyAccessTest, SetClampsToMinMax) {
	prop("clamped").set(&thing, 50.0);
	EXPECT_FLOAT_EQ(thing.clamped, 10.f);
	prop("clamped").set(&thing, -1.0);
	EXPECT_FLOAT_EQ(thing.clamped, 0.f);
}

TEST_F(PropertyAccessTest, EnumsAreValues) {
	EXPECT_EQ(prop("mode").get<LsTestMode>(&thing), LsTestMode::Fast);
	prop("mode").set(&thing, LsTestMode::Off);
	EXPECT_EQ(thing.mode, LsTestMode::Off);
}

TEST_F(PropertyAccessTest, StringsAndNamesAreStdStrings) {
	EXPECT_EQ(prop("label").get<std::string>(&thing), "thing");
	prop("label").set(&thing, std::string("renamed"));
	EXPECT_EQ(thing.label, "renamed");

	prop("tag").set(&thing, std::string("hello"));
	EXPECT_EQ(thing.tag, StringName("hello"));
	EXPECT_EQ(prop("tag").get<std::string>(&thing), "hello");
}

TEST_F(PropertyAccessTest, VectorsAndQuats) {
	EXPECT_EQ(prop("offset").get<glm::vec3>(&thing), glm::vec3(1, 2, 3));
	prop("offset").set(&thing, glm::vec3(4, 5, 6));
	EXPECT_EQ(thing.offset, glm::vec3(4, 5, 6));
	const glm::quat q(0, 1, 0, 0);
	prop("facing").set(&thing, q);
	EXPECT_EQ(thing.facing, q);
}

TEST_F(PropertyAccessTest, AssetsCarryTheirClass) {
	AssetRef ref = prop("asset").get<AssetRef>(&thing);
	EXPECT_EQ(ref.asset, nullptr);
	EXPECT_EQ(ref.type, &IAsset::StaticType); // the asset picker filters by it
	EXPECT_EQ(prop("asset").get<IAsset*>(&thing), nullptr);
}

TEST_F(PropertyAccessTest, ObjectHandlesAreEntityRefs) {
	prop("target").set(&thing, EntityRef{42});
	EXPECT_EQ(thing.target.handle, 42);
	EXPECT_EQ(prop("target").get<EntityRef>(&thing).handle, 42);
}

TEST_F(PropertyAccessTest, ValuesCompare) {
	PropAccessTestThing other;
	EXPECT_EQ(prop("offset").get_value(&thing), prop("offset").get_value(&other));
	other.offset.x = 9;
	EXPECT_NE(prop("offset").get_value(&thing), prop("offset").get_value(&other));
}
