#pragma once
#include <functional>
#include <unordered_map>
#include <string>

template<typename T>
class FnFactory
{
public:
	void add(std::string name, std::function<T* ()> func) {
		initializers.insert({ name,func });
	}
	T* create(std::string name) const {
		auto find = initializers.find(name);
		return find == initializers.end() ? nullptr : find->second();
	}
	void remove(std::string name) {
		initializers.erase(name);
	}
private:
	std::unordered_map<std::string, std::function<T*()>> initializers;
};
