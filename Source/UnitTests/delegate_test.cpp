#include <gtest/gtest.h>
#include "Framework/MulticastDelegate.h"

TEST(MulticastDelegateTest, InvokeAll)
{
    MulticastDelegate<int> d;
    int sum_a = 0, sum_b = 0;
    int key_a = 0, key_b = 0;
    d.add(&key_a, [&](int v) { sum_a += v; });
    d.add(&key_b, [&](int v) { sum_b += v; });
    d.invoke(5);
    EXPECT_EQ(sum_a, 5);
    EXPECT_EQ(sum_b, 5);
}

TEST(MulticastDelegateTest, RemoveListener)
{
    MulticastDelegate<> d;
    int count = 0;
    int key = 0;
    d.add(&key, [&]() { count++; });
    d.invoke();
    EXPECT_EQ(count, 1);
    d.remove(&key);
    d.invoke();
    EXPECT_EQ(count, 1);  // not fired again
}

TEST(MulticastDelegateTest, RemoveDuringInvoke)
{
    MulticastDelegate<> d;
    int count = 0;
    int key = 0;
    d.add(&key, [&]() {
        count++;
        d.remove(&key);  // remove self while firing
    });
    d.invoke();
    d.invoke();  // must not fire again
    EXPECT_EQ(count, 1);
}

TEST(MulticastDelegateTest, MemberFunctionBinding)
{
    struct Counter {
        int value = 0;
        void increment() { value++; }
    };
    Counter c;
    MulticastDelegate<> d;
    d.add(&c, &Counter::increment);
    d.invoke();
    d.invoke();
    EXPECT_EQ(c.value, 2);
}

TEST(MulticastDelegateTest, HasAnyListeners)
{
    MulticastDelegate<> d;
    EXPECT_FALSE(d.has_any_listeners());  // empty: no listeners
    int key = 0;
    d.add(&key, []() {});
    EXPECT_TRUE(d.has_any_listeners());
    d.remove(&key);
    EXPECT_FALSE(d.has_any_listeners());
}

TEST(MulticastDelegateTest, MultipleInvocations)
{
    MulticastDelegate<> d;
    int count = 0;
    int key = 0;
    d.add(&key, [&]() { count++; });
    for (int i = 0; i < 10; i++) d.invoke();
    EXPECT_EQ(count, 10);
}
