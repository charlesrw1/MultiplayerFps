#ifndef ANIMATION_H
#define ANIMATION_H
#include "Framework/Handle.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <memory>
#include "../AnimationTypes.h"

#include "Render/Model.h"
#include <vector>
#include "Framework/Factory.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"
#include "Animation/Runtime/SyncTime.h"
#include "Animation/AnimationSeqAsset.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/ConsoleCmdGroup.h"

#include <unordered_set>
#include <functional>
#include <variant>
#include "Animation/Editor/Optional.h"
#include <stdexcept>
using std::function;
class atGraphContext;
class Pose;
class Animator;
class MSkeleton;
class Entity;
class AnimationSeq;
class AnimTreePoseNode;
struct Rt_Vars_Base;

struct RootMotionTransform {
	glm::vec3 position_displacement=glm::vec3(0.f);
	glm::quat rotation_displacement=glm::quat();
};

// direct play slots for manual animation playback
struct DirectAnimationSlot {
	StringName name;
	const AnimationSeqAsset* active = nullptr;
	float time = 0.0;
	float playspeed = 1.0;
	bool apply_rootmotion = false;
	function<void(bool)> on_finished;

	float time_remaining() const;
};

// create this through code however you want
class agBaseNode;
class agBuilder : public ClassBase {
public:
	CLASS_BODY(agBuilder);
	
	REFLECT(lua_generic)
	agBaseNode* alloc(const ClassTypeInfo* info);
	REF void set_root(agBaseNode* node);

	template<typename T>
	T* alloc() {
		return (T*)alloc(&T::StaticType);
	}

	void add_cached_pose_root(agBaseNode* node);
	agBaseNode* get_root() const { return root; }
	std::vector<agBaseNode*>& get_cache_nodes() { return cachePoseNodes; }
	void add_slot_name(StringName name);
	std::vector<StringName>& get_slots() { return slot_names; }
private:
	agBaseNode* root = nullptr;
	std::vector<agBaseNode*> cachePoseNodes;
	std::vector<StringName> slot_names;
};

class ConstructorError : public std::runtime_error {
public:
	ConstructorError() : std::runtime_error("Constructor error.") {}
};
class agClipNode;
class AnimatorObject : public ClassBase {
public:
	CLASS_BODY(AnimatorObject);

	AnimatorObject(const Model& model, agBuilder& construction, Entity* ent = nullptr);
	~AnimatorObject();
	// Main update method
	void update(float dt);
	// what game/physics stuff consumes
	const std::vector<glm::mat4x4> get_global_bonemats() const { return cached_bonemats; }
	bool is_using_double_buffer() const { return using_global_bonemat_double_buffer; }
	const std::vector<glm::mat4x4> get_last_global_bonemats() const { return last_cached_bonemats; }
	const Model& get_model() const { return model; }
	const MSkeleton* get_skel() const { return model.get_skel(); }
	int num_bones() const { return cached_bonemats.size(); }
	Entity* get_owner() const { return owner; }
	bool play_animation(const AnimationSeqAsset* seq, float play_speed=1.f, float start_pos=0.f);
	// callback: returns true if interrupted, false if not. guaranteed to fire. 
	void play_animation(const AnimationSeqAsset* seq, float play_speed, float start_pos, function<void(bool)> callback);
	void stop_animation_in_slot(StringName slot);
	void add_simulating_physics_object(Entity* obj);
	void remove_simulating_physics_object(Entity* obj);
	void set_update_owner_position_to_root(bool b) { update_owner_position_to_root = b; }
	RootMotionTransform get_last_root_motion() const { return root_motion; }
	void set_matrix_palette_offset(int ofs) { matrix_palette_offset = ofs; }
	int get_matrix_palette_offset() const { return matrix_palette_offset; }
	bool get_is_for_editor() const { return get_owner() == nullptr; }
	opt<float> get_curve_value(StringName name) const;
	opt<float> get_float_variable(StringName name) const;
	opt<bool> get_bool_variable(StringName name) const;
	opt<int> get_int_variable(StringName name) const;
	opt<glm::vec3> get_vec3_variable(StringName name) const;
	REF void set_float_variable(StringName name, float f);
	REF void set_int_variable(StringName name, int f);
	void set_bool_variable(StringName name, bool f);
	void set_vec3_variable(StringName name, glm::vec3 f);
	agBaseNode* find_cached_pose_node(StringName name);
	agBaseNode& get_root_node() const;
	SyncGroupData& find_or_create_sync_group(StringName name);
	DirectAnimationSlot* find_slot_with_name(StringName name);
	void add_playing_clip(agClipNode* clip) { playingClipsThisUpdate.push_back(clip); }
	const std::vector<agClipNode*>& get_playing_clips() { return playingClipsThisUpdate; }
	
	// when debug printing enabled
	void debug_print(int start_y);
	void debug_enter_node(string msg) {
		debug_output_messages.push_back(string(cur_depth,' ')+msg);
		cur_depth++;
	}
	void debug_exit_node() {
		cur_depth--;
	}
private:
	int cur_depth = 0;
	vector<string> debug_output_messages;

	std::vector<agClipNode*> playingClipsThisUpdate;
	std::unordered_map<uint64_t, float> curve_values;
	std::unordered_map<uint64_t, std::variant<bool, float, int, glm::vec3>> blackboard;
	bool using_global_bonemat_double_buffer = true;
	vector<glm::mat4> cached_bonemats;	// global bonemats
	vector<glm::mat4> last_cached_bonemats;
	int matrix_palette_offset = 0;	// mesh space -> bone space -> meshspace, what the renderer consumes
	bool update_owner_position_to_root = false;
	// owning entity, can be null for example in editor
	Entity* owner = nullptr;
	const Model& model;
	RootMotionTransform root_motion;
	std::unordered_set<uint64_t> simulating_physics_objects;
	// active sync groups for graph
	vector<SyncGroupData> active_sync_groups;
	vector<DirectAnimationSlot> slots;
	agBuilder graph;

	bool update_sync_group(int idx);
	void update_slot(int idx, float dt);
	void update_physics_bones(const Pose& inpose);
	void ConcatWithInvPose();


	friend class NodeRt_Ctx;
	friend class EditModelAnimations;
	friend class AnimationEditorTool;
	friend class atGraphContext;
};


#endif