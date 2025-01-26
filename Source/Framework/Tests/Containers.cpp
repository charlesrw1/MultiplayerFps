#include "Framework/Hashmap.h"
#include "Test/Test.h"

ADD_TEST(Hashmap, Test)
{
	hash_map<void*> ptrs(4);
	for (int i = 1; i < 1000; i++) {
		ptrs.insert(i,(void*)i);
	}
	for (int i = 1; i < 1000; i++) {
		ptrs.remove(i);
		ptrs.remove(i+1);
	}
}