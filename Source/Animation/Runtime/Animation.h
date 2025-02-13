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
#include "Animation/AnimationSeqAsset.h"

#include <unordered_set>

struct RootMotionTransform
{
	glm::vec3 position_displacement=glm::vec3(0.f);
	glm::quat rotation_displacement=glm::quat();
};

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

	// what game/physics stuff consumes
	const std::vector<glm::mat4x4> get_global_bonemats() const {
		return cached_bonemats;
	}
	bool is_using_double_buffer() const {
		return using_global_bonemat_double_buffer;
	}
	const std::vector<glm::mat4x4> get_last_global_bonemats() const {
		return last_cached_bonemats;
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
	bool play_animation_in_slot(
		const AnimationSeqAsset* seq,
		StringName slot,
		float play_speed,
		float start_pos
	);
	void stop_animation_in_slot(
		StringName slot
	);

	void add_simulating_physics_object(Entity* obj);
	void remove_simulating_physics_object(Entity* obj);

	void set_update_owner_position_to_root(bool b) {
		update_owner_position_to_root = b;
	}

	RootMotionTransform get_last_root_motion() {
		return root_motion;
	}
	void set_matrix_palette_offset(int ofs) {
		matrix_palette_offset = ofs;
	}
	int get_matrix_palette_offset() const {
		return matrix_palette_offset;
	}
private:
	bool get_is_for_editor() const {
		return get_owner() == nullptr;
	}

	// hooks for derived classes
	virtual void on_init() {};
	virtual void on_update(float dt) {}
	virtual void on_post_update() {}

	std::vector<glm::mat4> cached_bonemats;	// global bonemats
	int matrix_palette_offset = 0;	// mesh space -> bone space -> meshspace, what the renderer consumes

	void ConcatWithInvPose();

	bool update_owner_position_to_root = false;
	
	bool using_global_bonemat_double_buffer = true;
	std::vector<glm::mat4> last_cached_bonemats;

	// owning entity, can be null for example in editor
	Entity* owner = nullptr;
	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;

	RootMotionTransform root_motion;

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
		const AnimationSeqAsset* active = nullptr;
		float lasttime = 0.0;
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