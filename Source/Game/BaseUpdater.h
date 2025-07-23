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
	REF void destroy_deferred();


	static const int INVALID_FILEID = 0;
	// Editor Data >>>>
	bool dont_serialize_or_edit = false;
	void set_editor_transient(bool transient) { dont_serialize_or_edit = transient; }

	bool dont_serialize_or_edit_this() const { return dont_serialize_or_edit; }
	//Entity* creator_source = nullptr;		// my creator
	//PrefabAsset* what_prefab = nullptr;	// (optional) what prefab created this (might be differnt than owner's prefab)
	int unique_file_id = INVALID_FILEID;			// unique id in source owner (either native c++, prefab, map)
	//bool is_root_of_prefab = false;
	// <<<<<<<<<<<<<<<<

	void post_unserialization(uint64_t id) {
		ASSERT(init_state == initialization_state::CONSTRUCTOR);
		this->instance_id = id;
		init_state = initialization_state::HAS_ID;
	}
	uint64_t get_instance_id() const { return instance_id; }
	bool is_activated() const { return init_state == initialization_state::CALLED_START; }
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
