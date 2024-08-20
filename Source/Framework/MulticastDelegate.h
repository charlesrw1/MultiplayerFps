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
	void add(T* instance, void (T::* memberFunction)(Args...), bool callOnce = false)
	{
		functions_[instance] = [instance, memberFunction](Args... args) {
			(instance->*memberFunction)(args...);
		};
	}
	template<typename T>
	void add_call_once(T* instance, void (T::* memberFunction)(Args...))
	{
		add<T>(instance, memberFunction, true);
	}
private:
	//a
	std::unordered_map<void*, std::function<void(Args...)>> functions_;
};
