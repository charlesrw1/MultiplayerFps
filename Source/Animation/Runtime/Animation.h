#ifndef ANIMATION_H
#define ANIMATION_H

#include "Framework/Handle.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>

#include "../AnimationTypes.h"
#include "../AnimationTreePublic.h"

#include "Model.h"
#include "Framework/ExpressionLang.h"
// procedural bone controls
// supports: direct bone transform manipulation	(ex: rotating/translating weapon bone)
//			ik to meshspace transform	(ex: hand reaching to object)
//			ik to transform relative to another bone	(ex: third person gun ik)
//			ik to relative transform of bone relative to another bone (ex: first person gun ik)
struct Bone_Controller
{
	bool enabled = false;
	// if true, then transform is added to base, not replaced
	bool add_transform_not_replace = false;
	// if true, then uses 2 bone ik instead of direct transform
	bool use_two_bone_ik = false;
	bool evalutate_in_second_pass = false;
	int bone_index = 0;
	float weight = 1.f;
	// target transform in meshspace!!!
	// so you should multiply by inv-transform matrix if you have a worldspace transform
	glm::quat rotation;
	glm::vec3 position;

	// if != -1, then position/rotation treated as a relative offset 
	// to another bone pre-procedural bone adjustments and not worldspace
	// useful for hand to gun ik
	int target_relative_bone_index = -1;
	bool use_bone_as_relative_transform = false;
};

// hardcoded bone controller types for programming convenience, doesnt affect any bone names/indicies
enum class bone_controller_type
{
	rhand,
	lhand,
	rfoot,
	lfoot,
	misc1,
	misc2,

	max_count,
};

#include <vector>
#include "Framework/Factory.h"
#include "Framework/ReflectionProp.h"
#include "Framework/TypeInfo.h"

#include "Framework/ClassBase.h"

#include "Animation/Runtime/SyncTime.h"

class Pose;
class Animator;
class MSkeleton;
class Entity;
class Animation_Tree_CFG;

CLASS_H(AnimatorInstance, ClassBase)
	AnimatorInstance();

	// returns true on success
	// fails if AnimatorInstance isnt compatible with AnimationGraph
	// or if model doesnt have skeleton
	bool initialize_animator(
		const Model* model,
		const Animation_Tree_CFG* graph,
		Entity* ent = nullptr);

	void tick_tree_new(float dt);

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

	Bone_Controller& get_controller(bone_controller_type type_) {
		return bone_controllers[(int)type_];
	}

	void play_anim_in_slot(StringView name, float start, float speed, bool loop);
	bool is_slot_finished() { return slots[0].finished; }

	void update_procedural_bones(Pose& pose);

	int num_bones() { return cached_bonemats.size(); }

	Entity* get_owner() const { return owner; }

	ScriptInstance& get_script_inst() { return script_inst; }

	bool is_initialized() const { return model != nullptr; }
private:

	// hooks for derived classes
	virtual void on_init() {};
	virtual void on_update(float dt) {}
	virtual void pre_ik_update(Pose& pose, float dt) {}
	virtual void post_ik_update() {}

	// owning entity, can be null for example in editor
	Entity* owner = nullptr;
	std::vector<glm::mat4> cached_bonemats;	// global bonemats
	std::vector<glm::mat4> matrix_palette;	// final transform matricies, meshspace->bonespace->meshspace

	void ConcatWithInvPose();
	void add_legs_layer(glm::quat q[], glm::vec3 pos[]);
	void UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies);
	
	// depreciated
	Bone_Controller bone_controllers[(int)bone_controller_type::max_count];
	std::vector<Animation_Slot> slots;

	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;

	ScriptInstance script_inst;
	std::vector<uint8_t> data;	// runtime data

	// active sync groups for graph
	std::vector<SyncGroupData> active_sync_groups;
	SyncGroupData& find_or_create_sync_group(StringName name);
	
	friend class NodeRt_Ctx;
};


#endif