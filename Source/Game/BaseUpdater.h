#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Util.h"

// Inherited by Entity and Component

class Entity;
class PrefabAsset;
class Level;

CLASS_H(BaseUpdater, ClassBase)
public:

	// initialization:
	// for obj in scene/prefab:
	//		obj->pre_start()
	// for obj in scene/prefab:
	//		obj->start()

	virtual void pre_start() {}
	virtual void start() {}
	virtual void update() {}
	virtual void end() {}

	void init_updater();
	void shutdown_updater();
	void set_ticking(bool shouldTick);

	// queues this entity/component to be destroyed at the end of the frame
	void destroy_deferred();

	static const PropertyInfoList* get_props();

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

	void set_call_init_in_editor(bool b) {
		call_init_in_editor = b;
	}
	bool get_call_init_in_editor() const {
		return call_init_in_editor;
	}

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
	bool is_type_for_script(const ClassTypeInfo* t) {
		if (!t) return false;
		return get_type().is_a(*t);
	}
	const ClassTypeInfo* get_type_for_script() {
		return &get_type();
	}

	void activate_internal_step1();
	void activate_internal_step2();
	void deactivate_internal();

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
	bool call_init_in_editor = false;
	bool tick_enabled = false;
};
