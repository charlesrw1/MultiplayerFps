#include <gtest/gtest.h>
#include "Framework/FreeList.h"

TEST(FreeListTest, MakeNewAndGet) {
	Free_List<int> fl;
	auto h = fl.make_new();
	EXPECT_TRUE(fl.check_handle(h));
	fl.get(h) = 42;
	EXPECT_EQ(fl.get(h), 42);
}

TEST(FreeListTest, MultipleObjects) {
	Free_List<int> fl;
	auto h0 = fl.make_new();
	auto h1 = fl.make_new();
	auto h2 = fl.make_new();
	fl.get(h0) = 10;
	fl.get(h1) = 20;
	fl.get(h2) = 30;
	EXPECT_EQ(fl.get(h0), 10);
	EXPECT_EQ(fl.get(h1), 20);
	EXPECT_EQ(fl.get(h2), 30);
	EXPECT_EQ((int)fl.objects.size(), 3);
}

TEST(FreeListTest, FreeReducesObjectCount) {
	Free_List<int> fl;
	auto h0 = fl.make_new();
	auto h1 = fl.make_new();
	fl.get(h0) = 1;
	fl.get(h1) = 2;
	fl.free(h0);
	EXPECT_EQ((int)fl.objects.size(), 1);
	EXPECT_FALSE(fl.check_handle(h0));
	EXPECT_TRUE(fl.check_handle(h1));
	EXPECT_EQ(fl.get(h1), 2);
}

TEST(FreeListTest, HandleReusedAfterFree) {
	Free_List<int> fl;
	auto h0 = fl.make_new();
	fl.free(h0);
	// The freed handle should be in free_handles
	EXPECT_EQ((int)fl.free_handles.size(), 1);
	// Allocating again should reuse h0
	auto h_new = fl.make_new();
	EXPECT_EQ(h_new, h0);
}

TEST(FreeListTest, DataPreservedAfterOtherFree) {
	Free_List<int> fl;
	auto h0 = fl.make_new();
	auto h1 = fl.make_new();
	auto h2 = fl.make_new();
	fl.get(h0) = 100;
	fl.get(h1) = 200;
	fl.get(h2) = 300;
	// Free the middle element; the back (h2) gets swapped in
	fl.free(h1);
	EXPECT_EQ(fl.get(h0), 100);
	EXPECT_EQ(fl.get(h2), 300);
	EXPECT_FALSE(fl.check_handle(h1));
}
