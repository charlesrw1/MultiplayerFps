#include <gtest/gtest.h>
#include <vector>
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

// Regression test: insert() used to stop at the first tombstone/empty slot it saw while
// probing, instead of scanning the whole probe chain for an existing match. That let it
// create a *second* entry for a handle that already existed further down the chain (behind
// a tombstone left by removing a different, colliding handle) instead of updating in place.
// Repro requires two handles that actually collide (same hash & mask), so we brute-force a
// colliding pair against the mask the table will use, rather than assuming specific values.
TEST(HashMapTest, InsertUpdateThroughTombstoneDoesNotDuplicate) {
	hash_map<int*> m(3); // 8 slots, mask = 7
	const uint64_t mask = 7;
	uint64_t hA = 0, hB = 0;
	for (uint64_t candidate = 1; hB == 0; candidate++) {
		uint64_t bucket = std::hash<uint64_t>()(candidate) & mask;
		if (hA == 0) {
			hA = candidate;
		} else if ((std::hash<uint64_t>()(hA) & mask) == bucket) {
			hB = candidate;
		}
		ASSERT_LT(candidate, 100000u) << "could not find a colliding pair";
	}

	int a = 1, b = 2, c = 3;
	m.insert(hA, &a); // takes the natural bucket
	m.insert(hB, &b); // collides, probes forward to the next slot
	m.remove(hA);     // tombstones the natural bucket, ahead of hB's real slot

	// "Update" hB while its natural bucket is a tombstone: must overwrite the existing
	// entry, not create a duplicate.
	m.insert(hB, &c);
	EXPECT_EQ(m.find(hB), &c);
	EXPECT_EQ(m.num_used, 1u);

	// Removing hB must make it unfindable — a leftover duplicate would resurrect stale data.
	m.remove(hB);
	EXPECT_EQ(m.find(hB), nullptr);
	EXPECT_EQ(m.num_used, 0u);
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

// Same regression as HashMapTest.InsertUpdateThroughTombstoneDoesNotDuplicate, but for
// hash_set: a colliding pointer must not get duplicated when its natural bucket is a
// tombstone left by a previously-removed, colliding pointer.
TEST(HashSetTest, InsertThroughTombstoneDoesNotDuplicate) {
	hash_set<int> s(3); // 8 slots, mask = 7
	const uint64_t mask = 7;
	std::vector<int> pool(100000);
	int* pA = nullptr;
	int* pB = nullptr;
	for (auto& v : pool) {
		uint64_t bucket = std::hash<uint64_t>()((uint64_t)&v) & mask;
		if (!pA) {
			pA = &v;
		} else if ((std::hash<uint64_t>()((uint64_t)pA) & mask) == bucket) {
			pB = &v;
			break;
		}
	}
	ASSERT_NE(pB, nullptr) << "could not find a colliding pair";

	s.insert(pA); // takes the natural bucket
	s.insert(pB); // collides, probes forward to the next slot
	s.remove(pA); // tombstones the natural bucket, ahead of pB's real slot

	s.insert(pB); // must be a no-op (already present), not a duplicate
	EXPECT_NE(s.find(pB), nullptr);
	EXPECT_EQ(s.num_used, 1);

	s.remove(pB);
	EXPECT_EQ(s.find(pB), nullptr);
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
