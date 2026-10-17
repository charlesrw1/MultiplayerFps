#include <gtest/gtest.h>
#include <cstddef>
#include "Framework/ClassBase.h"
#include "Framework/PropertyMake.h"
#include "Framework/PropertyPath.h"
#include "Framework/SerializerJson2.h"
#include "Testheader.h"

// List and struct properties: ListAccess, scoped struct borrows and PropertyPath.

namespace {
const PropertyInfo* find(const PropertyInfoList* props, std::string_view name) {
	EXPECT_NE(props, nullptr);
	for (int i = 0; i < props->count; i++)
		if (props->list[i].name == name)
			return &props->list[i];
	ADD_FAILURE() << "no property " << name;
	return nullptr;
}
const PropertyInfo* thing_prop(std::string_view name) {
	return find(PropListTestThing::StaticType.props, name);
}
const PropertyInfo* point_prop(std::string_view name) {
	return find(PropListTestPoint::StructType.properties, name);
}
} // namespace

class PropertyListTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); }
	void SetUp() override {
		thing.points.resize(3);
		for (int i = 0; i < 3; i++)
			thing.points[i].weight = float(i + 1);
		thing.samples = {1, 2, 3, 4};
		thing.names = {"a", "b", "c", "d"};
	}
	PropListTestThing thing;
};

TEST_F(PropertyListTest, ElementTypesAreDeduced) {
	const ListAccess& points = thing_prop("points")->list();
	EXPECT_EQ(points.element->type, core_type_id::ActualStruct);
	EXPECT_EQ(points.element->struct_type, &PropListTestPoint::StructType);
	EXPECT_EQ(thing_prop("samples")->list().element->type, core_type_id::Float);
	EXPECT_EQ(thing_prop("names")->list().element->type, core_type_id::StdString);
	EXPECT_EQ(thing_prop("pivot")->type, core_type_id::ActualStruct);
}

TEST_F(PropertyListTest, ValueElementsGetAndSet) {
	const PropertyInfo& samples = *thing_prop("samples");
	const ListAccess& list = samples.list();
	EXPECT_EQ(list.size(samples, &thing), 4);
	PropValue v;
	list.get(samples, &thing, 2, v);
	EXPECT_DOUBLE_EQ(prop_value_get<double>(v), 3.0);
	list.set(samples, &thing, 2, 9.5, SetMode::Edit);
	EXPECT_FLOAT_EQ(thing.samples[2], 9.5f);
	list.resize(samples, &thing, 6);
	EXPECT_EQ(thing.samples.size(), 6u);
}

TEST_F(PropertyListTest, MoveShiftsTheElementsBetween) {
	const PropertyInfo& names = *thing_prop("names");
	names.list().move(names, &thing, 0, 2);
	EXPECT_EQ(thing.names, (std::vector<std::string>{"b", "c", "a", "d"}));
	names.list().move(names, &thing, 3, 0);
	EXPECT_EQ(thing.names, (std::vector<std::string>{"d", "b", "c", "a"}));
}

TEST_F(PropertyListTest, StructElementsAreLentForOneCall) {
	const PropertyInfo& points = *thing_prop("points");
	points.list().with_element(points, &thing, 1, [](StructRef s) {
		EXPECT_EQ(s.type, &PropListTestPoint::StructType);
		struct_as<PropListTestPoint>(s).weight = 7.f;
	});
	EXPECT_FLOAT_EQ(thing.points[1].weight, 7.f);
}

TEST_F(PropertyListTest, SpanOnlyForPlainNumbers) {
	const PropertyInfo& samples = *thing_prop("samples");
	EXPECT_EQ(thing_prop("names")->list().with_span, nullptr);
	EXPECT_EQ(thing_prop("points")->list().with_span, nullptr);
	ASSERT_NE(samples.list().with_span, nullptr);
	size_t seen = 0;
	samples.list().with_span(samples, &thing, [&](void* data, size_t bytes) {
		seen = bytes;
		static_cast<float*>(data)[0] = 42.f;
	});
	EXPECT_EQ(seen, 4 * sizeof(float));
	EXPECT_FLOAT_EQ(thing.samples[0], 42.f);
}

TEST_F(PropertyListTest, PathsWalkStructsAndLists) {
	PropertyRef weight{&thing, PropertyPath(thing_prop("points")).element(2).child(point_prop("weight"))};
	EXPECT_EQ(weight.path.to_string(), "points[2].weight");
	EXPECT_DOUBLE_EQ(weight.get<double>(), 3.0);
	weight.set(0.5);
	EXPECT_FLOAT_EQ(thing.points[2].weight, 0.5f);

	PropertyRef tint{&thing, PropertyPath(thing_prop("pivot")).child(point_prop("tint"))};
	EXPECT_EQ(tint.path.to_string(), "pivot.tint");
	tint.set(COLOR_RED);
	EXPECT_EQ(thing.pivot.tint, COLOR_RED);
}

TEST_F(PropertyListTest, ListInsideAListElement) {
	thing.points[1].tags = {5, 6};
	PropertyRef tags{&thing, PropertyPath(thing_prop("points")).element(1).child(point_prop("tags"))};
	EXPECT_EQ(tags.list_size(), 2);
	tags.list_resize(3);
	PropertyRef third{&thing, tags.path.element(2)};
	EXPECT_EQ(third.path.to_string(), "points[1].tags[2]");
	third.set(11);
	EXPECT_EQ(thing.points[1].tags[2], 11);
	tags.list_move(2, 0);
	EXPECT_EQ(thing.points[1].tags, (std::vector<int>{11, 5, 6}));
}

TEST_F(PropertyListTest, PathsCheckIndicesOnEachAccess) {
	PropertyRef weight{&thing, PropertyPath(thing_prop("points")).element(2).child(point_prop("weight"))};
	EXPECT_TRUE(weight.resolves());
	thing.points.resize(2);
	EXPECT_FALSE(weight.resolves()); // the stored path does not dangle
	thing.points.resize(3);
	EXPECT_TRUE(weight.resolves());
}

TEST_F(PropertyListTest, ForEachValueVisitsNestedValues) {
	int numbers = 0, colors = 0, strings = 0;
	for_each_value(PropListTestThing::StaticType.props, &thing, [&](const PropertyInfo&, const PropValue& v) {
		numbers += std::holds_alternative<double>(v);
		colors += std::holds_alternative<Color32>(v);
		strings += std::holds_alternative<std::string>(v);
	});
	EXPECT_EQ(numbers, 3 + 4 + 1); // point weights, samples, pivot weight
	EXPECT_EQ(colors, 3 + 1);
	EXPECT_EQ(strings, 4);
}

TEST_F(PropertyListTest, InlineVecListsAreDeduced) {
	struct Local
	{
		InlineVec<int, 2> small;
	};
	const PropertyInfo p =
		make_property<&Local::small, checked_offset<offsetof(Local, small)>()>("small", PROP_DEFAULT);
	ASSERT_EQ(p.type, core_type_id::List);
	EXPECT_EQ(p.list().element->type, core_type_id::Int32);
	Local l;
	p.list().resize(p, &l, 3); // past the inline count
	EXPECT_EQ(l.small.size(), 3u);
	p.list().set(p, &l, 2, int64_t(8), SetMode::Edit);
	p.list().move(p, &l, 2, 0);
	EXPECT_EQ(l.small[0], 8);
}

TEST_F(PropertyListTest, ListsOfStructsRoundTrip) {
	thing.points[0].tags = {1, 2};
	thing.points[2].tint = COLOR_BLUE;
	thing.pivot.weight = 4.f;
	WriteSerializerBackendJson2 writer("test", thing);
	nlohmann::json json = writer.get_output();

	PropListTestThing copy;
	ReadSerializerBackendJson2 reader("test", json, copy);
	ASSERT_EQ(copy.points.size(), 3u);
	EXPECT_FLOAT_EQ(copy.points[1].weight, 2.f);
	EXPECT_EQ(copy.points[0].tags, (std::vector<int>{1, 2}));
	EXPECT_EQ(copy.points[2].tint, COLOR_BLUE);
	EXPECT_EQ(copy.samples, thing.samples);
	EXPECT_EQ(copy.names, thing.names);
	EXPECT_FLOAT_EQ(copy.pivot.weight, 4.f);
}
