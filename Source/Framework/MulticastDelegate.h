#pragma once
#include <functional>
template<typename... Args>
class MulticastDelegate
{
public:
	~MulticastDelegate();
	void add(void* key, std::function<void(Args...)> func);
	void remove(void* key);
	void invoke(Args... args);
	template<typename T>
	void add(T* instance, void (T::* memberFunction)(Args...));
	bool has_any_listeners() const { return !head; }
private:
	struct Item {
		void* key = nullptr;
		std::function<void(Args...)> func;
		Item* next = nullptr;
		bool in_func = false;
		bool wants_delete = false;
	};
	const Item* get_head() const { return head; }
	Item* find(Item* start, void* key);
	Item* head = nullptr;
};

template<typename ...Args>
inline MulticastDelegate<Args...>::~MulticastDelegate() {
	Item* ptr = head;
	while (ptr) {
		Item* next = ptr->next;
		delete ptr;
		ptr = next;
	}
}

template<typename ...Args>
inline void MulticastDelegate<Args...>::add(void* key, std::function<void(Args...)> func)
{
	Item* i = new Item;
	i->key = key;
	i->func = std::move(func);
	i->next = head;
	head = i;
}

template<typename ...Args>
inline void MulticastDelegate<Args...>::remove(void* key)
{
	Item* prev = nullptr;
	Item* ptr = head;
	while (ptr) {
		if (key == ptr->key) {
			if (ptr->in_func) {
				ptr->wants_delete = true;
			}
			else {
				if (prev)
					prev->next = ptr->next;
				else
					head = ptr->next;
				delete ptr;
				ptr = prev ? prev->next : head;
				continue;
			}
		}
		prev = ptr;
		ptr = ptr->next;
	}
	//printf("no matching key\n");
}

template<typename ...Args>
inline void MulticastDelegate<Args...>::invoke(Args ...args) {
	Item* prev = nullptr;
	Item* ptr = head;
	while (ptr)
	{
		ptr->in_func = true;
		ptr->func(args...);
		ptr->in_func = false;
		Item* next = ptr->next;
		if (ptr->wants_delete) {
			if (prev)
				prev->next = next;
			else
				head = next;
			delete ptr;
		}
		else {
			ptr->in_func = false;
			prev = ptr;
		}
		ptr = next;
	}
}

template<typename ...Args>
inline typename MulticastDelegate<Args...>::Item* MulticastDelegate<Args...>::find(Item* start, void* key) {
	Item* i = start;
	while (i) {
		if (i->key == key)
			return i;
		i = i->next;
	}
	return nullptr;
}

template<typename ...Args>
template<typename T>
inline void MulticastDelegate<Args...>::add(T* instance, void(T::* memberFunction)(Args...))
{
	add(instance, [instance, memberFunction](Args... args) {
		(instance->*memberFunction)(args...);
		});
}
