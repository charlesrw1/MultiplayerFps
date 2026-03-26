#include <gtest/gtest.h>
#include "Framework/PoolAllocator.h"

struct PoolTestObj
{
	int x = 0;
	int y = 0;
};

TEST(PoolAllocatorTest, AllocateReturnsNonNull) {
	Pool_Allocator<PoolTestObj> pool(8, "test");
	PoolTestObj* p = pool.allocate();
	EXPECT_NE(p, nullptr);
	pool.free(p);
}

TEST(PoolAllocatorTest, UsedObjectsTracking) {
	Pool_Allocator<PoolTestObj> pool(8, "test");
	EXPECT_EQ(pool.used_objects, 0);
	PoolTestObj* p1 = pool.allocate();
	EXPECT_EQ(pool.used_objects, 1);
	PoolTestObj* p2 = pool.allocate();
	EXPECT_EQ(pool.used_objects, 2);
	pool.free(p1);
	EXPECT_EQ(pool.used_objects, 1);
	pool.free(p2);
	EXPECT_EQ(pool.used_objects, 0);
}

TEST(PoolAllocatorTest, AllocatedPointersAreDistinct) {
	Pool_Allocator<PoolTestObj> pool(4, "test");
	PoolTestObj* p1 = pool.allocate();
	PoolTestObj* p2 = pool.allocate();
	PoolTestObj* p3 = pool.allocate();
	EXPECT_NE(p1, p2);
	EXPECT_NE(p2, p3);
	EXPECT_NE(p1, p3);
	pool.free(p1);
	pool.free(p2);
	pool.free(p3);
}

TEST(PoolAllocatorTest, ReuseAfterFree) {
	Pool_Allocator<PoolTestObj> pool(2, "test");
	PoolTestObj* p1 = pool.allocate();
	pool.free(p1);
	// After freeing p1, it should be available again
	PoolTestObj* p2 = pool.allocate();
	EXPECT_EQ(pool.used_objects, 1);
	pool.free(p2);
}

TEST(PoolAllocatorTest, FreeNullptrIsNoOp) {
	Pool_Allocator<PoolTestObj> pool(4, "test");
	pool.free(nullptr); // should not crash
	EXPECT_EQ(pool.used_objects, 0);
}

TEST(PoolAllocatorTest, ScopedPoolPtrFreesOnDestruction) {
	Pool_Allocator<PoolTestObj> pool(4, "test");
	{
		auto scoped = pool.allocate_scoped();
		EXPECT_EQ(pool.used_objects, 1);
		EXPECT_NE(scoped.get(), nullptr);
	}
	EXPECT_EQ(pool.used_objects, 0);
}

TEST(PoolAllocatorTest, ScopedPoolPtrMoveTransfersOwnership) {
	Pool_Allocator<PoolTestObj> pool(4, "test");
	auto s1 = pool.allocate_scoped();
	EXPECT_EQ(pool.used_objects, 1);
	{
		auto s2 = std::move(s1);
		EXPECT_EQ(pool.used_objects, 1);
		EXPECT_EQ(s1.get(), nullptr);
		EXPECT_NE(s2.get(), nullptr);
	}
	EXPECT_EQ(pool.used_objects, 0);
}
