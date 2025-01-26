#ifndef ANIMATION_H
#define ANIMATION_H

#include "Framework/Handle.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <memory>
#include "../AnimationTypes.h"
#include "../AnimationTreePublic.h"

#include "Render/Model.h"


#include <vector>
#include "Framework/Factory.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"
#include "Animation/Runtime/SyncTime.h"
#include "Framework/Hashset.h"

#include <unordered_set>

class Pose;
class Animator;
class MSkeleton;
class Entity;
class AnimationSeq;
class Animation_Tree_CFG;
class PhysicsComponentBase;
struct Rt_Vars_Base;
CLASS_H(AnimatorInstance, ClassBase)
public:
	AnimatorInstance();
	~AnimatorInstance();

	// returns true on success
	// fails if AnimatorInstance isnt compatible with AnimationGraph
	// or if model doesnt have skeleton
	bool initialize(
		const Model* model,
		const Animation_Tree_CFG* graph,
		Entity* ent = nullptr);

	// Main update method
	void update(float dt);

	// what renderer consumes
	const std::vector<glm::mat4x4> get_matrix_palette() const { 
		return matrix_palette; 
	}
	// what game/physics stuff consumes
	const std::vector<glm::mat4x4> get_global_bonemats() const {
		return cached_bonemats;
	}
	const Model* get_model() const {
		return model;
	}
	const MSkeleton* get_skel() const {
		return (model)?model->get_skel():nullptr;
	}
	const Animation_Tree_CFG* get_tree() const {
		return cfg;
	}
	int num_bones() const { return cached_bonemats.size(); }
	Entity* get_owner() const { return owner; }
	bool is_initialized() const { return model != nullptr; }

	// Slot playing
	// returns weather it was succesful
	bool play_animation_in_slot(
		std::string animation,
		StringName slot,
		float play_speed,
		float start_pos
	);

	void add_simulating_physics_object(Entity* obj);
	void remove_simulating_physics_object(Entity* obj);
private:

	// hooks for derived classes
	virtual void on_init() {};
	virtual void on_update(float dt) {}
	virtual void on_post_update() {}

	std::vector<glm::mat4> cached_bonemats;	// global bonemats
	std::vector<glm::mat4> matrix_palette;	// final transform matricies, meshspace->bonespace->meshspace

	void ConcatWithInvPose();
	
	// owning entity, can be null for example in editor
	Entity* owner = nullptr;
	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;

	std::vector<std::unique_ptr<Rt_Vars_Base>> runtime_graph_data;
	std::unordered_set<uint64_t> simulating_physics_objects;

	// active sync groups for graph
	std::vector<SyncGroupData> active_sync_groups;
	SyncGroupData& find_or_create_sync_group(StringName name);
	
	// direct play slots for manual animation playback
	struct DirectAnimationSlot
	{
		enum State {
			FadingIn,
			Full,
			FadingOut,
		}state;

		StringName name;

		const AnimationSeq* active = nullptr;
		float time = 0.0;
		float playspeed = 0.0;
		bool apply_rootmotion = false;

		float fade_duration = 0.2;
		float fade_percentage = 0.0;
	};
	DirectAnimationSlot* find_slot_with_name(StringName name) {
		for (int i = 0; i < slots.size(); i++)
			if (slots[i].name == name)
				return &slots[i];
		return nullptr;
	}

	std::vector<DirectAnimationSlot> slots;

	friend class NodeRt_Ctx;
	friend class EditModelAnimations;
	friend class AnimationEditorTool;
};


#endif