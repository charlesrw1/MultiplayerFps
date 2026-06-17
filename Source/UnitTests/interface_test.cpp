#include <gtest/gtest.h>
#include <string>
using std::string;
#include "Framework/ClassBase.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Testheader.h"

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
	// ClassTypeInfo::has_interface should be true for the type, verifying propagation
	EXPECT_TRUE(TestWithInterface::StaticType.has_interface(ITestInterface::StaticInterfaceType.id));
	EXPECT_TRUE(TestWithTwoInterfaces::StaticType.has_interface(ITestInterface::StaticInterfaceType.id));
	EXPECT_TRUE(TestWithTwoInterfaces::StaticType.has_interface(ISecondInterface::StaticInterfaceType.id));
}
