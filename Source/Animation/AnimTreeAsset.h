#pragma once
#include <json.hpp>
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Framework/StringName.h"
#include "Assets/IAsset.h"
#include "Animation/AnimationSeqAsset.h"
#include "Animation/Runtime/RuntimeNodesNew.h" // ModifyBoneType
#include "Animation/Runtime/Easing.h"
#include "Animation/Runtime/SyncTime.h" // sync_opt
#include "glm/glm.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Model;

// Authoring-time stand-in for the runtime ValueType (Animation/Runtime/RuntimeNodesNew2.h):
// every field that can be either a constant or a StringName-driven variable is one of these
// four structs, edited in the AnimTree editor as a single "mode dropdown + one field" property
// (see AnimTreeValueEditor in Editor/AnimTreeEditor.cpp) instead of two separate rows.
NEWENUM(AnimTreeValueMode, uint8_t){
	Constant,
	Variable,
};

struct AnimTreeFloatValue
{
	STRUCT_BODY();
	REF AnimTreeValueMode mode = AnimTreeValueMode::Constant;
	REF float constant = 0.f;
	REF StringName var_name;
};
struct AnimTreeIntValue
{
	STRUCT_BODY();
	REF AnimTreeValueMode mode = AnimTreeValueMode::Constant;
	REF int constant = 0;
	REF StringName var_name;
};
struct AnimTreeBoolValue
{
	STRUCT_BODY();
	REF AnimTreeValueMode mode = AnimTreeValueMode::Constant;
	REF bool constant = false;
	REF StringName var_name;
};
struct AnimTreeVec3Value
{
	STRUCT_BODY();
	REF AnimTreeValueMode mode = AnimTreeValueMode::Constant;
	REF glm::vec3 constant = glm::vec3(0.f);
	REF StringName var_name;
};

// One bone weight-paint entry, authored in order; later entries in the list win where subtrees
// overlap. See Editor/BoneMaskEditor.h for the widget that edits these, and AnimTreeBuild.cpp
// for how they're replayed onto agBlendMasked at build time.
struct BoneMaskEntry
{
	STRUCT_BODY();
	REF StringName bone;
	REF float weight = 1.f;
	REF bool include_children = true;
};

// -- Per-node-kind parameter structs. Field-for-field mirrors of the matching agXXXNode's
// -- constant/StringName-settable fields (Animation/Runtime/RuntimeNodesNew2.h). Reflected via
// -- STRUCT_BODY so they can be drawn directly by the generic PropertyGrid in the editor.

struct AnimTreeClipParams
{
	STRUCT_BODY();
	REF AssetPtr<AnimationSeqAsset> clip;
	REF bool looping = true;
	REF AnimTreeFloatValue speed;
	REFLECT(type=AnimTreeStringName)
	StringName sync_group;
	REF sync_opt sync_type = sync_opt::Default;
};
struct AnimTreeBlendParams
{
	STRUCT_BODY();
	REF AnimTreeFloatValue alpha;
};
struct AnimTreeBlendMaskedParams
{
	STRUCT_BODY();
	REF AnimTreeFloatValue alpha;
	REF bool meshspace_blend = false;
	REF float default_weight = 0.f;
	REF std::vector<BoneMaskEntry> mask;
};
struct AnimTreeAddParams
{
	STRUCT_BODY();
	REF AnimTreeFloatValue alpha;
};
struct AnimTreeMakeAdditiveParams
{
	STRUCT_BODY();
	// Binary: zero the delta for this bone + descendants (agMakeAdditive::mask_bone_and_children).
	REF std::vector<StringName> masked_bones;
};
struct AnimTreeIkParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName bone_name;
	REFLECT(type=AnimTreeStringName)
	StringName other_bone;
	REF bool take_rotation_of_other = false;
	REF bool ik_in_bone_space = false;
	REFLECT(type=AnimTreeStringName)
	StringName pole_bone;
	REF bool pole_in_bone_space = false;
	REF bool allow_stretching = false;
	REF float max_stretch_scale = 1.5f;
	REF float start_stretch_ratio = 1.f;
	REF AnimTreeFloatValue alpha;
	REF AnimTreeVec3Value target;
	REF AnimTreeVec3Value pole;
};
struct AnimTreeModifyBoneParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName bone_name;
	REF ModifyBoneType translation_mode = ModifyBoneType::None;
	REF ModifyBoneType rotation_mode = ModifyBoneType::None;
	REF ModifyBoneType scale_mode = ModifyBoneType::None;
	REF AnimTreeVec3Value translation;
	REF AnimTreeVec3Value rotation;
	REF AnimTreeVec3Value scale;
	REF AnimTreeFloatValue alpha;
};
struct AnimTreeCopyBoneParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName source_bone;
	REFLECT(type=AnimTreeStringName)
	StringName target_bone;
	REF AnimTreeBoolValue copy_translation;
	REF AnimTreeBoolValue copy_rotation;
	REF AnimTreeBoolValue copy_scale;
	REF bool copy_bonespace = false;
	REF AnimTreeFloatValue alpha;
};
struct AnimTreeSlotParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName slot_name;
	REF bool update_children_when_playing = false;
};
struct AnimTreeBlendByIntParams
{
	STRUCT_BODY();
	REF AnimTreeIntValue integer;
	REF Easing easing = Easing::CubicEaseIn;
	REF float blend_duration = 0.5f;
};
struct AnimTreeSaveCachedPoseParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName cache_name;
};
struct AnimTreeUseCachedPoseParams
{
	STRUCT_BODY();
	REFLECT(type=AnimTreeStringName)
	StringName cache_name;
};
// BindPose: no params, empty leaf.
struct AnimTreeBindPoseParams
{
	STRUCT_BODY();
};

enum class AnimTreeNodeKind : uint8_t
{
	Clip,
	BindPose,
	Blend,
	BlendMasked,
	Add,
	MakeAdditive,
	IK,
	ModifyBone,
	CopyBone,
	Slot,
	BlendByInt,
	SaveCachedPose,
	UseCachedPose,
};

using AnimTreeParams = std::variant<
	AnimTreeClipParams,
	AnimTreeBindPoseParams,
	AnimTreeBlendParams,
	AnimTreeBlendMaskedParams,
	AnimTreeAddParams,
	AnimTreeMakeAdditiveParams,
	AnimTreeIkParams,
	AnimTreeModifyBoneParams,
	AnimTreeCopyBoneParams,
	AnimTreeSlotParams,
	AnimTreeBlendByIntParams,
	AnimTreeSaveCachedPoseParams,
	AnimTreeUseCachedPoseParams>;

AnimTreeParams make_default_params_for_kind(AnimTreeNodeKind kind);
// Number of children a kind accepts: -1 means unbounded (BlendByInt only).
int get_arity_for_kind(AnimTreeNodeKind kind);
const char* get_kind_display_name(AnimTreeNodeKind kind);

// A single node in an AnimTree. Not a ClassBase/reflected leaf -- it's the tree container
// itself, hand-serialized (see to_json/from_json in AnimTreeAsset.cpp). Children are stored as
// unique_ptr so node addresses stay stable across sibling insert/erase/reorder -- the editor
// keys collapse state, selection, and drag payloads off `const AnimTreeNode*`.
struct AnimTreeNode
{
	std::string name; // optional; the "named node" query key handed back by AnimTreeBuild
	AnimTreeNodeKind kind = AnimTreeNodeKind::Clip;
	AnimTreeParams params = AnimTreeClipParams{};
	std::vector<std::unique_ptr<AnimTreeNode>> children;

	AnimTreeNode() = default;
	explicit AnimTreeNode(AnimTreeNodeKind k) : kind(k), params(make_default_params_for_kind(k)) {}

	std::unique_ptr<AnimTreeNode> clone() const;
};

void to_json(nlohmann::json& j, const AnimTreeNode& node);
void from_json(const nlohmann::json& j, AnimTreeNode& node);

class AnimTreeAsset : public IAsset
{
public:
	CLASS_BODY(AnimTreeAsset);

	REF static AnimTreeAsset* load(const std::string& name);

	bool load_asset() final;
	void post_load() final;
	void uninstall() final;

	void save_to_disk();

	// Reference model for bone-name pickers in the editor only; not used by AnimTreeBuild
	// unless the tree contains a BlendMasked/MakeAdditive/bone-name node -- see AnimTreeBuild.h.
	AssetPtr<Model> skeleton;
	std::unique_ptr<AnimTreeNode> root = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
	// name -> root, mirrors agSaveCachedPose roots registered via agBuilder::add_cached_pose_root.
	std::vector<std::pair<std::string, std::unique_ptr<AnimTreeNode>>> cached_pose_roots;
};
