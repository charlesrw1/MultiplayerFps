#include <gtest/gtest.h>
#include "Framework/RingBuffer.h"

TEST(RingBufferTest, Empty)
{
    RingBuffer<int> rb;
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.size(), 0);
}

TEST(RingBufferTest, BasicPushPop)
{
    RingBuffer<int> rb;
    rb.push_back(1);
    rb.push_back(2);
    rb.push_back(3);
    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb[0], 1);
    EXPECT_EQ(rb[1], 2);
    EXPECT_EQ(rb[2], 3);
    rb.pop_front();
    EXPECT_EQ(rb.size(), 2);
    EXPECT_EQ(rb[0], 2);
}

TEST(RingBufferTest, WrapAround)
{
    RingBuffer<int> rb(4);
    rb.push_back(1);
    rb.push_back(2);
    rb.push_back(3);
    rb.pop_front();  // remove 1
    rb.pop_front();  // remove 2
    rb.push_back(4);
    rb.push_back(5);
    // buffer: 3, 4, 5 (wraps around in backing array)
    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb[0], 3);
    EXPECT_EQ(rb[1], 4);
    EXPECT_EQ(rb[2], 5);
}

TEST(RingBufferTest, AutoGrow)
{
    RingBuffer<int> rb(2);  // start with 4 slots
    for (int i = 0; i < 20; i++) rb.push_back(i);
    EXPECT_EQ(rb.size(), 20);
    for (int i = 0; i < 20; i++) EXPECT_EQ(rb[i], i);
}

TEST(RingBufferTest, SingleElement)
{
    RingBuffer<int> rb;
    rb.push_back(42);
    EXPECT_FALSE(rb.empty());
    EXPECT_EQ(rb[0], 42);
    rb.pop_front();
    EXPECT_TRUE(rb.empty());
}

TEST(RingBufferTest, QueueOrdering)
{
    RingBuffer<int> rb;
    for (int i = 0; i < 5; i++) rb.push_back(i * 10);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(rb[0], i * 10);
        rb.pop_front();
    }
    EXPECT_TRUE(rb.empty());
}
