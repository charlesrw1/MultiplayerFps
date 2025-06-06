#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Framework/Reflection2.h"

// Inherited by Entity and Component

class Entity;
class PrefabAsset;
class Level;

class BaseUpdater : public ClassBase
{
public:
	CLASS_BODY(BaseUpdater);

	// queues this entity/component to be destroyed at the end of the frame
	REFLECT(name="destroy");
	void destroy_deferred();

	// Editor Data >>>>
	void set_editor_transient(bool transient) { editor_transient = true; }
	Entity* creator_source = nullptr;		// my creator
	PrefabAsset* what_prefab = nullptr;	// (optional) what prefab created this (might be differnt than owner's prefab)
	uint32_t unique_file_id = 0;			// unique id in source owner (either native c++, prefab, map)
	bool is_root_of_prefab = false;
	bool editor_transient = false;	// if true, dont serialize
	// <<<<<<<<<<<<<<<<
	bool dont_serialize_or_edit = false;



	void post_unserialization(uint64_t id) {
		ASSERT(init_state == initialization_state::CONSTRUCTOR);
		this->instance_id = id;
		init_state = initialization_state::HAS_ID;
	}
	uint64_t get_instance_id() const {
		return instance_id;
	}

	bool is_activated() const {
		return init_state == initialization_state::CALLED_START;
	}
protected:

	enum class initialization_state : uint8_t {
		CONSTRUCTOR,	// base state
		HAS_ID,		// recieve instance_id
		CALLED_PRE_START,
		CALLED_START		// fully initialized at this point
	};
	initialization_state init_state = initialization_state::CONSTRUCTOR;

	friend class UnserializedSceneFile;
	friend class SerializeTestWorkbench;
private:
	uint64_t instance_id = 0;	// instance id
};
