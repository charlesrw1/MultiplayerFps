#include <gtest/gtest.h>
#include "Framework/Hashmap.h"
#include "Framework/Hashset.h"

// ---- hash_map ----
// hash_map<T> maps uint64 handles -> T (designed for pointer values; 0 is INVALID_HANDLE)

TEST(HashMapTest, InsertAndFind) {
	hash_map<int*> m;
	int val = 42;
	m.insert(1, &val);
	EXPECT_EQ(m.find(1), &val);
	EXPECT_EQ(m.find(2), nullptr);
	EXPECT_EQ(m.find(0), nullptr); // 0 == INVALID_HANDLE, always nullptr
}

TEST(HashMapTest, Remove) {
	hash_map<int*> m;
	int a = 1, b = 2;
	m.insert(10, &a);
	m.insert(20, &b);
	m.remove(10);
	EXPECT_EQ(m.find(10), nullptr);
	EXPECT_EQ(m.find(20), &b);
}

TEST(HashMapTest, UpdateExistingHandle) {
	hash_map<int*> m;
	int a = 1, b = 2;
	m.insert(5, &a);
	m.insert(5, &b);
	EXPECT_EQ(m.find(5), &b);
	EXPECT_EQ(m.num_used, 1u); // still one entry
}

TEST(HashMapTest, RehashOnManyInserts) {
	hash_map<int*> m(2); // start tiny (4 slots)
	const int N = 50;
	int vals[N];
	for (int i = 0; i < N; i++) {
		vals[i] = i;
		m.insert((uint64_t)(i + 1), &vals[i]);
	}
	for (int i = 0; i < N; i++) {
		EXPECT_EQ(m.find((uint64_t)(i + 1)), &vals[i]) << "missing handle " << i + 1;
	}
}

TEST(HashMapTest, ClearAll) {
	hash_map<int*> m;
	int v = 1;
	m.insert(1, &v);
	m.clear_all();
	EXPECT_EQ(m.find(1), nullptr);
	EXPECT_EQ(m.num_used, 0u);
}

TEST(HashMapTest, TombstoneDoesNotBlockLookup) {
	// Insert two items that might land in adjacent slots, remove first, find second.
	hash_map<int*> m(3); // 8 slots
	int a = 1, b = 2;
	m.insert(1, &a);
	m.insert(2, &b);
	m.remove(1);
	EXPECT_EQ(m.find(2), &b);
}

// ---- hash_set ----
// hash_set<T> stores T* pointers; key IS the pointer value.

TEST(HashSetTest, InsertFindRemove) {
	hash_set<int> s;
	int a = 10, b = 20;
	s.insert(&a);
	s.insert(&b);
	EXPECT_NE(s.find(&a), nullptr);
	EXPECT_NE(s.find(&b), nullptr);
	s.remove(&a);
	EXPECT_EQ(s.find(&a), nullptr);
	EXPECT_NE(s.find(&b), nullptr);
}

TEST(HashSetTest, DuplicateInsertIsIdempotent) {
	hash_set<int> s;
	int a = 1;
	s.insert(&a);
	s.insert(&a);
	EXPECT_EQ(s.num_used, 1);
}

TEST(HashSetTest, ClearAll) {
	hash_set<int> s;
	int a = 1, b = 2;
	s.insert(&a);
	s.insert(&b);
	s.clear_all();
	EXPECT_EQ(s.find(&a), nullptr);
	EXPECT_EQ(s.num_used, 0);
}

TEST(HashSetTest, Iteration) {
	hash_set<int> s;
	int a = 1, b = 2, c = 3;
	s.insert(&a);
	s.insert(&b);
	s.insert(&c);
	int count = 0;
	for (int* p : s) {
		(void)p;
		count++;
	}
	EXPECT_EQ(count, 3);
}
