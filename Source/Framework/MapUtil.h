#pragma once
#include <unordered_map>
#include <cassert>
#include <unordered_set>
class MapUtil
{
public:
	template<typename T, typename K>
	static bool contains(const std::unordered_map<K, T>&, const K& key);
	template<typename T,typename K>
	static T get_or(const std::unordered_map<K, T>&, const K& key, T orval);
	template<typename T, typename K>
	static T get_or_null(const std::unordered_map<K, T>&, const K& key);
	template<typename T, typename K>
	static const T* get_opt(const std::unordered_map<K, T>&, const K& key);
	template<typename T, typename K>
	static void insert_test_exists(std::unordered_map<K, T>&, const K& key, T value);
};
class SetUtil
{
public:
	template<typename T, typename K>
	static bool contains(const std::unordered_set<K, T>&, const K& key);
	template<typename T, typename K>
	static void insert_test_exists(std::unordered_set<K, T>&, const K& key);
};

template<typename T, typename K>
inline bool MapUtil::contains(const std::unordered_map<K, T>& map, const K& key)
{
	return map.find(key) != map.end();
}
template<typename T, typename K>
inline T MapUtil::get_or(const std::unordered_map<K, T>& map, const K& key, T orval)
{
	auto find = map.find(key);
	return find == map.end() ? orval : find->second;
}
template<typename T, typename K>
inline T MapUtil::get_or_null(const std::unordered_map<K, T>& map, const K& key)
{
	auto find = map.find(key);
	return find == map.end() ? nullptr : find->second;
}
template<typename T, typename K>
inline const T* MapUtil::get_opt(const std::unordered_map<K, T>& map, const K& key)
{
	auto find = map.find(key);
	return find == map.end() ? nullptr : &find->second;
}

template<typename T, typename K>
inline void MapUtil::insert_test_exists(std::unordered_map<K, T>& map, const K& key, T value)
{
	assert(!contains(map, key));
	map.insert({ key,value });
}

template<typename T, typename K>
inline bool SetUtil::contains(const std::unordered_set<K, T>& set, const K& key)
{
	return set.find(key) != set.end();
}
template<typename T, typename K>
inline void SetUtil::insert_test_exists(std::unordered_set<K, T>& set, const K& key)
{
	assert(!contains(set, key));
	set.insert({ key });
}
