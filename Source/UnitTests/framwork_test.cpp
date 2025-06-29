#include "Unittest.h"
#include <unordered_map>
#include <unordered_set>
#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"

arena_string to_arstring(double d, ArenaScope& scope)
{
	const auto len = static_cast<size_t>(_scprintf("%f", d));
	arena_string str(len, '\0',scope);
	sprintf_s(&str[0], len + 1, "%f", d);
	return str;
}
arena_string to_arstring(int64_t d, ArenaScope& scope)
{
	const auto len = static_cast<size_t>(_scprintf("%lld", d));
	arena_string str(len, '\0', scope);
	sprintf_s(&str[0], len + 1, "%lld", d);
	return str;
}

ADD_TEST(allocator)
{

	Memory_Arena arena;
	arena.init("", 10000);
	for(int i=0;i<100;i++)
	{
		ArenaScope scope(arena,ArenaScope::BOTTOM);

	
		arena_map<arena_string, int> map(scope);
		arena_set<int> theset(scope);

		//for (int j = 0; j < 100; j++)
		//	map.insert({ arena_string("hello",scope)+ to_arstring((int64_t)j,scope),j});

		arena_string mystr("my string that should allocate", scope);
		arena_string other = mystr + "abc";
		string out(other);


		arena_vec<float> floats(10,1.f, scope);

		arena_vec<int> vec(scope);

		vec.resize(1000);
	}
	arena.shutdown();
}