#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Util.h"

// Inherited by Entity and Component

class Entity;
class PrefabAsset;
class Level;

CLASS_H(BaseUpdater, ClassBase)
public:
	virtual void update() {}

	void init_updater();
	void shutdown_updater();
	void set_ticking(bool shouldTick);

	static const PropertyInfoList* get_props() {
		START_PROPS(BaseUpdater)
			REG_BOOL(tickEnabled,PROP_DEFAULT,"0")
		END_PROPS(BaseUpdater)
	}

	uint64_t instance_id = 0;	// instance id
	bool tickEnabled = false;

	// Editor Data >>>>
	void set_editor_transient(bool transient) { editor_transient = true; }
	Entity* creator_source = nullptr;		// my creator
	PrefabAsset* what_prefab = nullptr;	// (optional) what prefab created this (might be differnt than owner's prefab)
	uint32_t unique_file_id = 0;			// unique id in source owner (either native c++, prefab, map)
	bool is_root_of_prefab = false;
	bool editor_transient = false;	// if true, dont serialize
	bool is_native_created = false;
	// <<<<<<<<<<<<<<<<
	bool dont_serialize_or_edit = false;

	void post_unserialization(uint64_t id) {
		ASSERT(init_state == initialization_state::CONSTRUCTOR);
		this->instance_id = id;
		init_state = initialization_state::HAS_ID;
	}

protected:
	enum class initialization_state : uint8_t {
		CONSTRUCTOR,	// base state
		HAS_ID,		// recieve instance_id
		INITIALIZED,	// initialize called
	};
	initialization_state init_state = initialization_state::CONSTRUCTOR;

	friend class UnserializedSceneFile;
	friend class SerializeTestWorkbench;
};
