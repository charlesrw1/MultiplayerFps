#pragma once

#include <functional>
#include <unordered_map>

template<typename... Args>
class MulticastDelegate
{
public:
	void add(void* key, std::function<void(Args...)> func)
	{
		functions_[key] = func;
	}
	void remove(void* key)
	{
		functions_.erase(key);
	}
	void invoke(Args... args) {
		for (const auto& pair : functions_)
		{
			pair.second(args...);
		}
	}
	template<typename T>
	void add(T* instance, void (T::* memberFunction)(Args...))
	{
		functions_[instance] = [instance, memberFunction](Args... args) {
			(instance->*memberFunction)(args...);
		};
	}
private:
	std::unordered_map<void*, std::function<void(Args...)>> functions_;
};