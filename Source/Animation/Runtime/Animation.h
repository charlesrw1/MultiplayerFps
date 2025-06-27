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
#include "Framework/MulticastDelegate.h"
#include "Framework/ConsoleCmdGroup.h"
#include "Animation/Runtime/RuntimeNodesBase.h"
#include <unordered_set>
#include <functional>
using std::function;
class atGraphContext;
class Pose;
class Animator;
class MSkeleton;
class Entity;
class AnimationSeq;
class Animation_Tree_CFG;
class AnimTreePoseNode;
struct Rt_Vars_Base;

struct RootMotionTransform {
	glm::vec3 position_displacement=glm::vec3(0.f);
	glm::quat rotation_displacement=glm::quat();
};

// direct play slots for manual animation playback
struct DirectAnimationSlot {
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
	float fade_percentage = 0.0;
	function<void(bool)> on_finished;
};

// subclass this to allow graph to reference variables and such
class AnimatorObject;
class Model;
class Entity;
class AnimatorInstance : public ClassBase {
public:
	CLASS_BODY(AnimatorInstance);
	AnimatorInstance();
	~AnimatorInstance();
	Entity* get_owner() const;
	AnimatorObject* get_obj() const;
	const Model* get_model() const;
	const MSkeleton* get_skel() const;
	virtual void on_init() {};
	virtual void on_update(float dt) {}
	virtual void on_post_update() {}
private:
	AnimatorObject* object = nullptr;
	friend class AnimatorObject;
};

class AnimatorObject : public ClassBase {
public:
	// throws on failure
	AnimatorObject(const Model& model, const Animation_Tree_CFG& graph, Entity* ent = nullptr);
	~AnimatorObject();
	// Main update method
	void update(float dt);
	// what game/physics stuff consumes
	const std::vector<glm::mat4x4> get_global_bonemats() const { return cached_bonemats; }
	bool is_using_double_buffer() const { return using_global_bonemat_double_buffer; }
	const std::vector<glm::mat4x4> get_last_global_bonemats() const { return last_cached_bonemats; }
	const Model& get_model() const { return model; }
	const MSkeleton* get_skel() const { return model.get_skel(); }
	const Animation_Tree_CFG& get_tree() const { return cfg; }
	int num_bones() const { return cached_bonemats.size(); }
	Entity* get_owner() const { return owner; }
	bool play_animation_in_slot(const AnimationSeqAsset* seq, StringName slot, float play_speed, float start_pos);
	// callback: returns true if interrupted, false if not. guaranteed to fire. 
	void play_animation_in_slot(const AnimationSeqAsset* seq, StringName slot, float play_speed, float start_pos, function<void(bool)> callback);
	void stop_animation_in_slot(StringName slot);
	void add_simulating_physics_object(Entity* obj);
	void remove_simulating_physics_object(Entity* obj);
	void set_update_owner_position_to_root(bool b) { update_owner_position_to_root = b; }
	RootMotionTransform get_last_root_motion() const { return root_motion; }
	void set_matrix_palette_offset(int ofs) { matrix_palette_offset = ofs; }
	int get_matrix_palette_offset() const { return matrix_palette_offset; }
	bool get_is_for_editor() const { return get_owner() == nullptr; }
	AnimatorInstance* get_instance() const { return animator.get(); }
#ifdef EDITOR_BUILD
	void set_force_seq_for_editor(const AnimationSeqAsset* seq) { force_view_seq = seq; }
	void set_force_view_seq_time(float t) { force_view_seq_time = t; }
#endif
private:
#ifdef EDITOR_BUILD
	const AnimationSeqAsset* force_view_seq{};
	float force_view_seq_time = 0.0;
#endif
	bool using_global_bonemat_double_buffer = true;
	vector<glm::mat4> cached_bonemats;	// global bonemats
	vector<glm::mat4> last_cached_bonemats;
	int matrix_palette_offset = 0;	// mesh space -> bone space -> meshspace, what the renderer consumes
	bool update_owner_position_to_root = false;
	// owning entity, can be null for example in editor
	Entity* owner = nullptr;
	const Animation_Tree_CFG& cfg;
	const Model& model;
	RootMotionTransform root_motion;
	vector<uptr<PoseNodeInst>> pose_node_insts;
	std::unordered_set<uint64_t> simulating_physics_objects;
	// active sync groups for graph
	vector<SyncGroupData> active_sync_groups;
	vector<DirectAnimationSlot> slots;
	uptr<AnimatorInstance> animator;

	bool update_sync_group(int idx);
	void update_slot(int idx, float dt);
	void update_physics_bones(const Pose& inpose);

	PoseNodeInst& get_root_node() const;
	SyncGroupData& find_or_create_sync_group(StringName name);
	void ConcatWithInvPose();
	DirectAnimationSlot* find_slot_with_name(StringName name);

	friend class NodeRt_Ctx;
	friend class EditModelAnimations;
	friend class AnimationEditorTool;
	friend class atGraphContext;
};


#endif