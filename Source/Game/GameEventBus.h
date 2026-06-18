#pragma once
#include <unordered_map>
#include <functional>
#include "Framework/InlineVec.h"
#include "Framework/ClassBase.h"
class Entity;
class ClassBase;

struct GameEvent {
	int event_id = 0;
	Entity* sender = nullptr;
	const char* payload = "";
};
class LuaEventBusFunction : public ClassBase
{
public:
	CLASS_BODY(LuaEventBusFunction);
	REF virtual void invoke(Entity* invoker, std::string payload) {}
};

class GameEventBus {
public:
	using Callback = std::function<void(const GameEvent&)>;

	static GameEventBus* get();

	// per-entity: listen for event_id on a specific sender
	void listen(int event_id, Entity* sender, ClassBase* listener, Callback cb);

	// global: listen for event_id from any sender
	void listen_global(int event_id, ClassBase* listener, Callback cb);

	// broadcast event_id from sender — hits targeted listeners then global listeners
	void broadcast(int event_id, Entity* sender, const char* payload = "");

	// remove a specific listener from a specific {event_id, sender}
	void remove(int event_id, Entity* sender, ClassBase* listener);

	// remove all listeners registered by this object
	void remove_all(ClassBase* listener);

	// remove all listeners targeting this entity (call from Entity::destroy_internal)
	void remove_listeners(Entity* sender);

	// clear everything (call on level change)
	void clear();

private:
	struct Listener {
		ClassBase* who = nullptr;
		Callback cb;
	};

	struct EntityEventKey {
		int64_t entity_handle = 0;
		int event_id = 0;

		bool operator==(const EntityEventKey& o) const {
			return entity_handle == o.entity_handle && event_id == o.event_id;
		}
	};

	struct EntityEventKeyHash {
		size_t operator()(const EntityEventKey& k) const {
			size_t h = std::hash<int64_t>{}(k.entity_handle);
			h ^= std::hash<int>{}(k.event_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	// {entity_handle, event_id} -> listeners
	std::unordered_map<EntityEventKey, InlineVec<Listener, 2>, EntityEventKeyHash> targeted;

	// event_id -> global listeners
	std::unordered_map<int, InlineVec<Listener, 2>> global;

	// inverse: listener -> list of keys it's registered under (for fast remove_all)
	struct Registration {
		EntityEventKey key;
		bool is_global = false;
	};
	std::unordered_map<ClassBase*, InlineVec<Registration, 4>> inverse;
};
