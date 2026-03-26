#include <gtest/gtest.h>
#include "Framework/Handle.h"
#include "Framework/ScopedBoolean.h"
#include "Framework/Range.h"
#include <vector>

// ---- handle<T> ----

TEST(HandleTest, DefaultIsInvalid) {
	handle<int> h;
	EXPECT_FALSE(h.is_valid());
	EXPECT_EQ(h.id, -1);
}

TEST(HandleTest, ValidAfterAssignment) {
	handle<int> h;
	h.id = 5;
	EXPECT_TRUE(h.is_valid());
}

TEST(HandleTest, ZeroIdIsValid) {
	handle<int> h;
	h.id = 0;
	EXPECT_TRUE(h.is_valid());
}

// ---- ScopedBooleanValue / BooleanScope ----

TEST(ScopedBooleanTest, DefaultIsFalse) {
	ScopedBooleanValue v;
	EXPECT_FALSE(v.get_value());
	EXPECT_FALSE((bool)v);
}

TEST(ScopedBooleanTest, TrueDuringScope) {
	ScopedBooleanValue v;
	{
		BooleanScope scope(v);
		EXPECT_TRUE(v.get_value());
		EXPECT_TRUE((bool)v);
	}
	EXPECT_FALSE(v.get_value());
}

TEST(ScopedBooleanTest, RestoredAfterScopeExit) {
	ScopedBooleanValue v;
	{ BooleanScope scope(v); }
	EXPECT_FALSE(v.get_value());
}

// ---- Range ----

TEST(RangeTest, IteratesCorrectIndices) {
	std::vector<int> indices;
	for (int i : Range(5)) {
		indices.push_back(i);
	}
	ASSERT_EQ((int)indices.size(), 5);
	for (int i = 0; i < 5; i++) {
		EXPECT_EQ(indices[i], i);
	}
}

TEST(RangeTest, ZeroRange) {
	int count = 0;
	for (int i : Range(0)) {
		(void)i;
		count++;
	}
	EXPECT_EQ(count, 0);
}

TEST(RangeTest, ConstructFromVector) {
	std::vector<int> v = {10, 20, 30};
	int count = 0;
	for (int i : Range(v)) {
		EXPECT_LT(i, (int)v.size());
		count++;
	}
	EXPECT_EQ(count, 3);
}

// ---- IPairs ----

TEST(IPairsTest, IndexAndPointerCorrect) {
	std::vector<int> v = {100, 200, 300};
	int expected_idx = 0;
	for (auto d : IPairs<int>(v)) {
		EXPECT_EQ(d.index, expected_idx);
		EXPECT_EQ(*d.data, v[expected_idx]);
		expected_idx++;
	}
	EXPECT_EQ(expected_idx, 3);
}

TEST(IPairsTest, MutateViaPointer) {
	std::vector<int> v = {1, 2, 3};
	for (auto d : IPairs<int>(v)) {
		*d.data *= 10;
	}
	EXPECT_EQ(v[0], 10);
	EXPECT_EQ(v[1], 20);
	EXPECT_EQ(v[2], 30);
}
