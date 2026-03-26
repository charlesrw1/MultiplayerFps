#include <gtest/gtest.h>
#include "Framework/InlineVec.h"
#include <string>

TEST(InlineVecTest, PushBackInline) {
	InlineVec<int, 4> v;
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	EXPECT_EQ(v.size(), 3);
	EXPECT_EQ(v[0], 1);
	EXPECT_EQ(v[2], 3);
}

TEST(InlineVecTest, HeapTransitionOnOverflow) {
	InlineVec<int, 2> v;
	v.push_back(10);
	v.push_back(20);
	v.push_back(30); // spills onto heap
	EXPECT_EQ(v.size(), 3);
	EXPECT_EQ(v[0], 10);
	EXPECT_EQ(v[1], 20);
	EXPECT_EQ(v[2], 30);
}

TEST(InlineVecTest, ManyPushBacks) {
	InlineVec<int, 4> v;
	for (int i = 0; i < 100; i++)
		v.push_back(i);
	ASSERT_EQ(v.size(), 100);
	for (int i = 0; i < 100; i++)
		EXPECT_EQ(v[i], i);
}

TEST(InlineVecTest, CopyInline) {
	InlineVec<int, 4> a;
	a.push_back(10);
	a.push_back(20);
	InlineVec<int, 4> b = a;
	EXPECT_EQ(b.size(), 2);
	EXPECT_EQ(b[0], 10);
	EXPECT_EQ(b[1], 20);
	b[0] = 99;
	EXPECT_EQ(a[0], 10); // deep copy: a unchanged
}

TEST(InlineVecTest, CopyAfterHeapSpill) {
	InlineVec<int, 2> a;
	for (int i = 0; i < 5; i++)
		a.push_back(i);
	InlineVec<int, 2> b = a;
	ASSERT_EQ(b.size(), 5);
	for (int i = 0; i < 5; i++)
		EXPECT_EQ(b[i], i);
}

TEST(InlineVecTest, AssignOperator) {
	InlineVec<int, 4> a, b;
	a.push_back(1);
	a.push_back(2);
	b = a;
	EXPECT_EQ(b.size(), 2);
	EXPECT_EQ(b[0], 1);
}

TEST(InlineVecTest, ResizeShrink) {
	InlineVec<int, 4> v;
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.resize(1);
	EXPECT_EQ(v.size(), 1);
	EXPECT_EQ(v[0], 1);
}

TEST(InlineVecTest, ResizeGrowWithinInlineCapacity) {
	InlineVec<int, 8> v;
	v.resize(4, 7);
	EXPECT_EQ(v.size(), 4);
	for (int i = 0; i < 4; i++)
		EXPECT_EQ(v[i], 7);
}

TEST(InlineVecTest, ResizeGrowBeyondCapacity) {
	// Previously buggy: resize used (curcount+1 > cap) check instead of (newcount > cap),
	// causing a buffer overflow when resizing from empty to > inline capacity.
	InlineVec<int, 2> v;
	v.resize(8, 5);
	ASSERT_EQ(v.size(), 8);
	for (int i = 0; i < 8; i++)
		EXPECT_EQ(v[i], 5);
}

TEST(InlineVecTest, ClearResetsSize) {
	InlineVec<int, 4> v;
	v.push_back(1);
	v.push_back(2);
	v.clear();
	EXPECT_EQ(v.size(), 0);
}

TEST(InlineVecTest, NonTrivialDestructor) {
	// Ensure non-trivially-destructible types are handled correctly.
	InlineVec<std::string, 2> v;
	v.push_back("hello");
	v.push_back("world");
	v.push_back("overflow"); // spills to heap
	EXPECT_EQ(v.size(), 3);
	EXPECT_EQ(v[0], "hello");
	EXPECT_EQ(v[2], "overflow");
	// destructor must call ~string() on all elements - no leak/crash
}

TEST(InlineVecTest, GetSpan) {
	InlineVec<int, 4> v;
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	auto s = v.get_span();
	EXPECT_EQ(s.size(), 3u);
	EXPECT_EQ(s[0], 1);
	EXPECT_EQ(s[2], 3);
}
