#ifndef ANIMATION_H
#define ANIMATION_H

#include "Parameter.h"
#include "Handle.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <string>
#include <memory>


class Model;
using std::vector;
using std::unique_ptr;

struct AnimChannel
{
	int num_positions = 0;
	int num_rotations = 0;
	int num_scales = 0;

	// Indicies, not file pointers
	int pos_start = 0;
	int rot_start = 0;
	int scale_start = 0;
};

template<typename T>
struct KeyframeBase 
{
	T val;
	float time;
};

typedef KeyframeBase<glm::vec3> PosKeyframe;
typedef KeyframeBase<glm::vec3> ScaleKeyframe;
typedef KeyframeBase<glm::quat> RotKeyframe;


class Animation
{
public:
	std::string name;
	float total_duration = 0;
	bool looping = false;
	float fps = 24;

	int channel_offset=0;
	int pos_offset=0;
	int num_pos = 0;
	int rot_offset=0;
	int num_rot = 0;
	int scale_offset=0;
	int num_scale = 0;
	// end - start
	glm::vec3 root_motion_translation;
};

class Animation_Set
{
public:
	int find(const char* animation_name) const;

	// Returns index to use with GetX(), -1 if no keyframes
	int FirstPositionKeyframe(float frame, int channel, int clip) const;
	int FirstRotationKeyframe(float frame, int channel, int clip) const;
	int FirstScaleKeyframe(float frame, int channel, int clip) const;
	const PosKeyframe& GetPos(int channel, int index, int clip) const;
	const RotKeyframe& GetRot(int channel, int index, int clip) const;
	const ScaleKeyframe& GetScale(int channel, int index, int clip) const;
	const AnimChannel& GetChannel(int clip, int channel) const;
public:
	int num_channels = 0;
	std::vector<AnimChannel> channels;
	std::vector<PosKeyframe> positions;
	std::vector<RotKeyframe> rotations;
	std::vector<ScaleKeyframe> scales;
	std::vector<Animation> clips;
};

struct Animator_Layer
{
	int anim = -1;
	float frame = 0;
	float speed = 1;
	bool finished = false;
	bool loop = false;
	int blend_anim = -1;
	float blend_frame = 1;
	float blend_remaining = 0;
	float blend_time = 0;

	// horrible hacky fix for networking
	int staging_anim = -1;
	float staging_frame = 0;
	bool staging_loop = false;
	float staging_speed = 1.f;

	void update(float dt, const Animation& clip);
};


class Pose
{
public:
	glm::quat q[256];
	glm::vec3 pos[256];
};

class PoseMask
{
public:
	PoseMask();
	void clear_all();
	bool is_masked(int index);
	void set(int index, bool masked);
	uint64_t mask[4];	//256/64
};

struct playing_clip
{
	int index;
	float frame;
	bool looping = false;
};



// hardcoded layers with masks
enum class animlayer
{
	armleft,
	armright,
	legleft,
	legright,
	upperbody,
	lowerbody,
	fullbody,
	additive_misc
};
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


class Entity;
class Animation_Tree_CFG;
class Animation_Tree_RT;
class Animation_Set_New;
class Animator
{
public:
	Animator();

	void set_model(const Model* model);
	void set_anim(const char* name, bool restart, float blend = 0.1f);
	void set_anim_from_index(Animator_Layer& l, int animation, bool restart, float blend = 0.1f);
	void set_leg_anim(const char* name, bool restart, float blend = 0.1f);
	void SetupBones();
	void ConcatWithInvPose();

	void tick_tree_new(float dt);

	// what renderer consumes
	const std::vector<glm::mat4x4> get_matrix_palette() const { 
		return matrix_palette; 
	}
	// what game/physics stuff consumes
	const std::vector<glm::mat4x4> get_global_bonemats() const {
		return cached_bonemats;
	}

	void AdvanceFrame(float elapsed_time);

	void set_model_new(const Model* model);
	void reset_new();
	void evaluate_new(float dt);

	Animator_Layer m;		// main animimation, upper body
	Animator_Layer legs;	// legs

	const Model* model = nullptr;
	const Animation_Set* set = nullptr;
	const Animation_Set_New* set_n = nullptr;

	Entity* owner = nullptr;
	
	Animation_Tree_RT* tree = nullptr;

	handle<Parameter> p_flMovex;
	handle<Parameter> p_flMovey;
	handle<Parameter> p_flSpeed;
	handle<Parameter> p_bCrouch;
	handle<Parameter> p_bJumping;
	handle<Parameter> p_bFalling;
	handle<Parameter> p_bMoving;


	Bone_Controller& get_controller(bone_controller_type type_) {
		return bone_controllers[(int)type_];
	}
	Bone_Controller bone_controllers[(int)bone_controller_type::max_count];
	void update_procedural_bones(Pose& pose);

	struct input {
		glm::vec3 worldpos;
		glm::vec3 worldrot;

		glm::vec3 desireddir;
		glm::vec3 facedir;
		glm::vec3 velocity;
		glm::vec2 groundvelocity;
		glm::vec2 relmovedir;	//forwards/back etc.
		glm::vec2 relaccel;
		
		bool reset_accel = true;
		glm::quat player_rot_from_accel = glm::quat(1, 0, 0, 1);

		bool crouched;
		bool falling;
		bool injump;
		bool ismoving;

		bool use_rhik;
		bool use_lhik;
		glm::vec3 rhandtarget;
		glm::vec3 lhandtarget;
		bool use_headlook;
		glm::vec3 headlooktarget;
	}in;

	struct output {
		glm::vec3 meshoffset;
		glm::vec3 rootmotion_angles_delta;
		glm::vec3 rootmotion_position_delta;
	}out;

	int num_bones() { return cached_bonemats.size(); }
private:
	void postprocess_animation(Pose& pose,float dt);

	//void DoHandIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);
	//void DoPlayerHandToGunIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);

	vector<glm::mat4x4> cached_bonemats;	// global bonemats
	vector<glm::mat4> matrix_palette;	// final transform matricies, meshspace->bonespace->meshspace

	//void CalcRotations(glm::quat q[], glm::vec3 pos[], int clip_index, float curframe);

	// lerps 1->2 and outputs to 1
	//void LerpTransforms(glm::quat q1[], glm::vec3 p1[], glm::quat q2[], glm::vec3 p2[], float factor, int numbones);

	void add_legs_layer(glm::quat q[], glm::vec3 pos[]);

	void UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies);
};


#endif