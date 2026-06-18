#include "GameEventBus.h"
#include "Game/Entity.h"
#include <algorithm>

static GameEventBus g_event_bus;
GameEventBus* GameEventBus::get() { return &g_event_bus; }

template<typename T, uint32_t N>
static void swap_erase(InlineVec<T, N>& vec, int index) {
	ASSERT(index >= 0 && index < vec.size());
	int last = vec.size() - 1;
	if (index != last)
		vec[index] = vec[last];
	vec.resize(last);
}

template<typename T, uint32_t N, typename Pred>
static bool swap_erase_first(InlineVec<T, N>& vec, Pred pred) {
	for (int i = 0; i < vec.size(); i++) {
		if (pred(vec[i])) {
			swap_erase(vec, i);
			return true;
		}
	}
	return false;
}

void GameEventBus::listen(int event_id, Entity* sender, ClassBase* listener, Callback cb) {
	ASSERT(sender && listener && cb);
	EntityEventKey key{sender->get_instance_id(), event_id};
	targeted[key].push_back({listener, cb});
	inverse[listener].push_back({key, false});
}

void GameEventBus::listen_global(int event_id, ClassBase* listener, Callback cb) {
	ASSERT(listener && cb);
	global[event_id].push_back({listener, cb});
	EntityEventKey dummy{0, event_id};
	inverse[listener].push_back({dummy, true});
}

void GameEventBus::broadcast(int event_id, Entity* sender, const char* payload) {
	ASSERT(sender);
	GameEvent event;
	event.event_id = event_id;
	event.sender = sender;
	event.payload = payload;

	EntityEventKey key{sender->get_instance_id(), event_id};
	auto it = targeted.find(key);
	if (it != targeted.end()) {
		auto& vec = it->second;
		for (int i = 0; i < vec.size(); i++)
			vec[i].cb(event);
	}

	auto git = global.find(event_id);
	if (git != global.end()) {
		auto& vec = git->second;
		for (int i = 0; i < vec.size(); i++)
			vec[i].cb(event);
	}
}

void GameEventBus::remove(int event_id, Entity* sender, ClassBase* listener) {
	ASSERT(sender && listener);
	EntityEventKey key{sender->get_instance_id(), event_id};

	auto it = targeted.find(key);
	if (it != targeted.end()) {
		swap_erase_first(it->second, [&](const Listener& l) { return l.who == listener; });
		if (it->second.size() == 0)
			targeted.erase(it);
	}

	auto inv_it = inverse.find(listener);
	if (inv_it != inverse.end()) {
		swap_erase_first(inv_it->second, [&](const Registration& r) {
			return !r.is_global && r.key == key;
		});
		if (inv_it->second.size() == 0)
			inverse.erase(inv_it);
	}
}

void GameEventBus::remove_all(ClassBase* listener) {
	auto inv_it = inverse.find(listener);
	if (inv_it == inverse.end())
		return;

	// copy out since we're modifying the map
	auto regs_copy = inv_it->second;
	inverse.erase(inv_it);

	for (int i = 0; i < regs_copy.size(); i++) {
		auto& reg = regs_copy[i];
		if (reg.is_global) {
			auto git = global.find(reg.key.event_id);
			if (git != global.end()) {
				swap_erase_first(git->second, [&](const Listener& l) { return l.who == listener; });
				if (git->second.size() == 0)
					global.erase(git);
			}
		} else {
			auto it = targeted.find(reg.key);
			if (it != targeted.end()) {
				swap_erase_first(it->second, [&](const Listener& l) { return l.who == listener; });
				if (it->second.size() == 0)
					targeted.erase(it);
			}
		}
	}
}

void GameEventBus::remove_listeners(Entity* sender) {
	int64_t handle = sender->get_instance_id();

	auto it = targeted.begin();
	while (it != targeted.end()) {
		if (it->first.entity_handle == handle) {
			auto& vec = it->second;
			for (int i = 0; i < vec.size(); i++) {
				auto inv_it = inverse.find(vec[i].who);
				if (inv_it != inverse.end()) {
					swap_erase_first(inv_it->second, [&](const Registration& r) {
						return !r.is_global && r.key == it->first;
					});
					if (inv_it->second.size() == 0)
						inverse.erase(inv_it);
				}
			}
			it = targeted.erase(it);
		} else {
			++it;
		}
	}
}

void GameEventBus::clear() {
	targeted.clear();
	global.clear();
	inverse.clear();
}
