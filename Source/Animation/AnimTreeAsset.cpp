#include "AnimTreeAsset.h"
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"

class AnimTreeAssetMetadata : public AssetMetadata
{
public:
	AnimTreeAssetMetadata() { extensions.push_back("animtree"); }
	Color32 get_browser_color() const override { return {120, 190, 255}; }
	std::string get_type_name() const override { return "AnimTree"; }
	const ClassTypeInfo* get_asset_class_type() const override { return &AnimTreeAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(AnimTreeAssetMetadata);
#endif

// ---- kind <-> arity / defaults / display ----

int get_arity_for_kind(AnimTreeNodeKind kind) {
	switch (kind) {
	case AnimTreeNodeKind::Clip:
	case AnimTreeNodeKind::BindPose:
	case AnimTreeNodeKind::UseCachedPose: // resolves to another named node by StringName, not a child
		return 0;
	case AnimTreeNodeKind::Blend:
	case AnimTreeNodeKind::BlendMasked:
	case AnimTreeNodeKind::Add:
	case AnimTreeNodeKind::MakeAdditive: // input (motion clip) + reference (subtracted pose)
		return 2;
	case AnimTreeNodeKind::IK:
	case AnimTreeNodeKind::ModifyBone:
	case AnimTreeNodeKind::CopyBone:
	case AnimTreeNodeKind::Slot:
	case AnimTreeNodeKind::SaveCachedPose:
		return 1;
	case AnimTreeNodeKind::BlendByInt:
		return -1;
	}
	return 0;
}

AnimTreeParams make_default_params_for_kind(AnimTreeNodeKind kind) {
	switch (kind) {
	case AnimTreeNodeKind::Clip: {
		AnimTreeClipParams p;
		p.speed.constant = 1.f;
		return p;
	}
	case AnimTreeNodeKind::BindPose: return AnimTreeBindPoseParams{};
	case AnimTreeNodeKind::Blend: return AnimTreeBlendParams{};
	case AnimTreeNodeKind::BlendMasked: return AnimTreeBlendMaskedParams{};
	case AnimTreeNodeKind::Add: return AnimTreeAddParams{};
	case AnimTreeNodeKind::MakeAdditive: return AnimTreeMakeAdditiveParams{};
	case AnimTreeNodeKind::IK: return AnimTreeIkParams{};
	case AnimTreeNodeKind::ModifyBone: return AnimTreeModifyBoneParams{};
	case AnimTreeNodeKind::CopyBone: return AnimTreeCopyBoneParams{};
	case AnimTreeNodeKind::Slot: return AnimTreeSlotParams{};
	case AnimTreeNodeKind::BlendByInt: return AnimTreeBlendByIntParams{};
	case AnimTreeNodeKind::SaveCachedPose: return AnimTreeSaveCachedPoseParams{};
	case AnimTreeNodeKind::UseCachedPose: return AnimTreeUseCachedPoseParams{};
	}
	return AnimTreeClipParams{};
}

const char* get_kind_display_name(AnimTreeNodeKind kind) {
	switch (kind) {
	case AnimTreeNodeKind::Clip: return "Clip";
	case AnimTreeNodeKind::BindPose: return "BindPose";
	case AnimTreeNodeKind::Blend: return "Blend";
	case AnimTreeNodeKind::BlendMasked: return "BlendMasked";
	case AnimTreeNodeKind::Add: return "Add";
	case AnimTreeNodeKind::MakeAdditive: return "MakeAdditive";
	case AnimTreeNodeKind::IK: return "IK";
	case AnimTreeNodeKind::ModifyBone: return "ModifyBone";
	case AnimTreeNodeKind::CopyBone: return "CopyBone";
	case AnimTreeNodeKind::Slot: return "Slot";
	case AnimTreeNodeKind::BlendByInt: return "BlendByInt";
	case AnimTreeNodeKind::SaveCachedPose: return "SaveCachedPose";
	case AnimTreeNodeKind::UseCachedPose: return "UseCachedPose";
	}
	return "?";
}

static AnimTreeNodeKind kind_from_string(const std::string& s) {
	for (int i = 0; i <= (int)AnimTreeNodeKind::UseCachedPose; i++) {
		if (s == get_kind_display_name((AnimTreeNodeKind)i))
			return (AnimTreeNodeKind)i;
	}
	return AnimTreeNodeKind::Clip;
}

// ---- AnimTreeNode::clone ----

std::unique_ptr<AnimTreeNode> AnimTreeNode::clone() const {
	auto out = std::make_unique<AnimTreeNode>();
	out->name = name;
	out->kind = kind;
	out->params = params;
	out->children.reserve(children.size());
	for (auto& c : children)
		out->children.push_back(c->clone());
	return out;
}

// ---- JSON: AnimTreeValue / BoneMaskEntry ----

static void to_json(nlohmann::json& j, const AnimTreeValueMode& m) { j = (m == AnimTreeValueMode::Variable) ? "Variable" : "Constant"; }
static void from_json(const nlohmann::json& j, AnimTreeValueMode& m) { m = (j.get<std::string>() == "Variable") ? AnimTreeValueMode::Variable : AnimTreeValueMode::Constant; }
static void to_json(nlohmann::json& j, const StringName& s) { j = std::string(s.get_c_str() ? s.get_c_str() : ""); }
static void from_json(const nlohmann::json& j, StringName& s) { s = StringName(j.get<std::string>().c_str()); }
// Not named to_json/from_json: glm::vec3 lives in namespace glm, so nlohmann's ADL lookup
// (which only searches the argument types' own associated namespaces, not global) would never
// find a global-namespace to_json/from_json for it -- see EditorRecents.cpp's vec3_to_json for
// the same workaround already used elsewhere in this codebase.
static nlohmann::json vec3_to_json(const glm::vec3& v) {
	nlohmann::json j;
	j["x"] = v.x; j["y"] = v.y; j["z"] = v.z;
	return j;
}
static glm::vec3 vec3_from_json(const nlohmann::json& j) {
	return glm::vec3(j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f));
}

static void to_json(nlohmann::json& j, const AnimTreeFloatValue& v) { j["mode"] = v.mode; j["constant"] = v.constant; j["var_name"] = v.var_name; }
static void from_json(const nlohmann::json& j, AnimTreeFloatValue& v) {
	if (j.contains("mode")) j.at("mode").get_to(v.mode);
	v.constant = j.value("constant", 0.f);
	if (j.contains("var_name")) j.at("var_name").get_to(v.var_name);
}
static void to_json(nlohmann::json& j, const AnimTreeIntValue& v) { j["mode"] = v.mode; j["constant"] = v.constant; j["var_name"] = v.var_name; }
static void from_json(const nlohmann::json& j, AnimTreeIntValue& v) {
	if (j.contains("mode")) j.at("mode").get_to(v.mode);
	v.constant = j.value("constant", 0);
	if (j.contains("var_name")) j.at("var_name").get_to(v.var_name);
}
static void to_json(nlohmann::json& j, const AnimTreeBoolValue& v) { j["mode"] = v.mode; j["constant"] = v.constant; j["var_name"] = v.var_name; }
static void from_json(const nlohmann::json& j, AnimTreeBoolValue& v) {
	if (j.contains("mode")) j.at("mode").get_to(v.mode);
	v.constant = j.value("constant", false);
	if (j.contains("var_name")) j.at("var_name").get_to(v.var_name);
}
static void to_json(nlohmann::json& j, const AnimTreeVec3Value& v) { j["mode"] = v.mode; j["constant"] = vec3_to_json(v.constant); j["var_name"] = v.var_name; }
static void from_json(const nlohmann::json& j, AnimTreeVec3Value& v) {
	if (j.contains("mode")) j.at("mode").get_to(v.mode);
	if (j.contains("constant")) v.constant = vec3_from_json(j.at("constant"));
	if (j.contains("var_name")) j.at("var_name").get_to(v.var_name);
}
static void to_json(nlohmann::json& j, const BoneMaskEntry& e) { j["bone"] = e.bone; j["weight"] = e.weight; j["include_children"] = e.include_children; }
static void from_json(const nlohmann::json& j, BoneMaskEntry& e) {
	if (j.contains("bone")) j.at("bone").get_to(e.bone);
	e.weight = j.value("weight", 1.f);
	e.include_children = j.value("include_children", true);
}

// ---- JSON: per-kind params ----

static void to_json(nlohmann::json& j, const AnimTreeClipParams& p) {
	j["clip"] = p.clip.get() ? p.clip.get()->get_name() : std::string();
	j["looping"] = p.looping;
	j["speed"] = p.speed;
	j["sync_group"] = p.sync_group;
	j["sync_type"] = (int)p.sync_type;
}
static void from_json(const nlohmann::json& j, AnimTreeClipParams& p) {
	std::string clip_path = j.value("clip", std::string());
	p.clip = clip_path.empty() ? AssetPtr<AnimationSeqAsset>{} : g_assets.find<AnimationSeqAsset>(clip_path);
	p.looping = j.value("looping", true);
	if (j.contains("speed")) j.at("speed").get_to(p.speed);
	if (j.contains("sync_group")) j.at("sync_group").get_to(p.sync_group);
	p.sync_type = (sync_opt)j.value("sync_type", (int)sync_opt::Default);
}
static void to_json(nlohmann::json& j, const AnimTreeBindPoseParams&) { j = nlohmann::json::object(); }
static void from_json(const nlohmann::json&, AnimTreeBindPoseParams&) {}
static void to_json(nlohmann::json& j, const AnimTreeBlendParams& p) { j["alpha"] = p.alpha; }
static void from_json(const nlohmann::json& j, AnimTreeBlendParams& p) { if (j.contains("alpha")) j.at("alpha").get_to(p.alpha); }
static void to_json(nlohmann::json& j, const AnimTreeBlendMaskedParams& p) {
	j["alpha"] = p.alpha;
	j["meshspace_blend"] = p.meshspace_blend;
	j["default_weight"] = p.default_weight;
	j["mask"] = p.mask;
}
static void from_json(const nlohmann::json& j, AnimTreeBlendMaskedParams& p) {
	if (j.contains("alpha")) j.at("alpha").get_to(p.alpha);
	p.meshspace_blend = j.value("meshspace_blend", false);
	p.default_weight = j.value("default_weight", 0.f);
	if (j.contains("mask")) j.at("mask").get_to(p.mask);
}
static void to_json(nlohmann::json& j, const AnimTreeAddParams& p) { j["alpha"] = p.alpha; }
static void from_json(const nlohmann::json& j, AnimTreeAddParams& p) { if (j.contains("alpha")) j.at("alpha").get_to(p.alpha); }
static void to_json(nlohmann::json& j, const AnimTreeMakeAdditiveParams& p) { j["masked_bones"] = p.masked_bones; }
static void from_json(const nlohmann::json& j, AnimTreeMakeAdditiveParams& p) { if (j.contains("masked_bones")) j.at("masked_bones").get_to(p.masked_bones); }
static void to_json(nlohmann::json& j, const AnimTreeIkParams& p) {
	j["bone_name"] = p.bone_name;
	j["other_bone"] = p.other_bone;
	j["take_rotation_of_other"] = p.take_rotation_of_other;
	j["ik_in_bone_space"] = p.ik_in_bone_space;
	j["pole_bone"] = p.pole_bone;
	j["pole_in_bone_space"] = p.pole_in_bone_space;
	j["allow_stretching"] = p.allow_stretching;
	j["max_stretch_scale"] = p.max_stretch_scale;
	j["start_stretch_ratio"] = p.start_stretch_ratio;
	j["alpha"] = p.alpha;
	j["target"] = p.target;
	j["pole"] = p.pole;
}
static void from_json(const nlohmann::json& j, AnimTreeIkParams& p) {
	if (j.contains("bone_name")) j.at("bone_name").get_to(p.bone_name);
	if (j.contains("other_bone")) j.at("other_bone").get_to(p.other_bone);
	p.take_rotation_of_other = j.value("take_rotation_of_other", false);
	p.ik_in_bone_space = j.value("ik_in_bone_space", false);
	if (j.contains("pole_bone")) j.at("pole_bone").get_to(p.pole_bone);
	p.pole_in_bone_space = j.value("pole_in_bone_space", false);
	p.allow_stretching = j.value("allow_stretching", false);
	p.max_stretch_scale = j.value("max_stretch_scale", 1.5f);
	p.start_stretch_ratio = j.value("start_stretch_ratio", 1.f);
	if (j.contains("alpha")) j.at("alpha").get_to(p.alpha);
	if (j.contains("target")) j.at("target").get_to(p.target);
	if (j.contains("pole")) j.at("pole").get_to(p.pole);
}
static void to_json(nlohmann::json& j, const ModifyBoneType& m) { j = (int)m; }
static void from_json(const nlohmann::json& j, ModifyBoneType& m) { m = (ModifyBoneType)j.get<int>(); }
static void to_json(nlohmann::json& j, const AnimTreeModifyBoneParams& p) {
	j["bone_name"] = p.bone_name;
	j["translation_mode"] = p.translation_mode;
	j["rotation_mode"] = p.rotation_mode;
	j["scale_mode"] = p.scale_mode;
	j["translation"] = p.translation;
	j["rotation"] = p.rotation;
	j["scale"] = p.scale;
	j["alpha"] = p.alpha;
}
static void from_json(const nlohmann::json& j, AnimTreeModifyBoneParams& p) {
	if (j.contains("bone_name")) j.at("bone_name").get_to(p.bone_name);
	if (j.contains("translation_mode")) j.at("translation_mode").get_to(p.translation_mode);
	if (j.contains("rotation_mode")) j.at("rotation_mode").get_to(p.rotation_mode);
	if (j.contains("scale_mode")) j.at("scale_mode").get_to(p.scale_mode);
	if (j.contains("translation")) j.at("translation").get_to(p.translation);
	if (j.contains("rotation")) j.at("rotation").get_to(p.rotation);
	if (j.contains("scale")) j.at("scale").get_to(p.scale);
	if (j.contains("alpha")) j.at("alpha").get_to(p.alpha);
}
static void to_json(nlohmann::json& j, const AnimTreeCopyBoneParams& p) {
	j["source_bone"] = p.source_bone;
	j["target_bone"] = p.target_bone;
	j["copy_translation"] = p.copy_translation;
	j["copy_rotation"] = p.copy_rotation;
	j["copy_scale"] = p.copy_scale;
	j["copy_bonespace"] = p.copy_bonespace;
	j["alpha"] = p.alpha;
}
static void from_json(const nlohmann::json& j, AnimTreeCopyBoneParams& p) {
	if (j.contains("source_bone")) j.at("source_bone").get_to(p.source_bone);
	if (j.contains("target_bone")) j.at("target_bone").get_to(p.target_bone);
	if (j.contains("copy_translation")) j.at("copy_translation").get_to(p.copy_translation);
	if (j.contains("copy_rotation")) j.at("copy_rotation").get_to(p.copy_rotation);
	if (j.contains("copy_scale")) j.at("copy_scale").get_to(p.copy_scale);
	p.copy_bonespace = j.value("copy_bonespace", false);
	if (j.contains("alpha")) j.at("alpha").get_to(p.alpha);
}
static void to_json(nlohmann::json& j, const AnimTreeSlotParams& p) { j["slot_name"] = p.slot_name; j["update_children_when_playing"] = p.update_children_when_playing; }
static void from_json(const nlohmann::json& j, AnimTreeSlotParams& p) {
	if (j.contains("slot_name")) j.at("slot_name").get_to(p.slot_name);
	p.update_children_when_playing = j.value("update_children_when_playing", false);
}
static void to_json(nlohmann::json& j, const Easing& e) { j = (int)e; }
static void from_json(const nlohmann::json& j, Easing& e) { e = (Easing)j.get<int>(); }
static void to_json(nlohmann::json& j, const AnimTreeBlendByIntParams& p) { j["integer"] = p.integer; j["easing"] = p.easing; j["blend_duration"] = p.blend_duration; }
static void from_json(const nlohmann::json& j, AnimTreeBlendByIntParams& p) {
	if (j.contains("integer")) j.at("integer").get_to(p.integer);
	if (j.contains("easing")) j.at("easing").get_to(p.easing);
	p.blend_duration = j.value("blend_duration", 0.5f);
}
static void to_json(nlohmann::json& j, const AnimTreeSaveCachedPoseParams& p) { j["cache_name"] = p.cache_name; }
static void from_json(const nlohmann::json& j, AnimTreeSaveCachedPoseParams& p) { if (j.contains("cache_name")) j.at("cache_name").get_to(p.cache_name); }
static void to_json(nlohmann::json& j, const AnimTreeUseCachedPoseParams& p) { j["cache_name"] = p.cache_name; }
static void from_json(const nlohmann::json& j, AnimTreeUseCachedPoseParams& p) { if (j.contains("cache_name")) j.at("cache_name").get_to(p.cache_name); }

static void params_to_json(nlohmann::json& j, AnimTreeNodeKind kind, const AnimTreeParams& params) {
	std::visit([&](auto&& p) { to_json(j, p); }, params);
}
static void params_from_json(const nlohmann::json& j, AnimTreeNodeKind kind, AnimTreeParams& out) {
	out = make_default_params_for_kind(kind);
	std::visit([&](auto&& p) { from_json(j, p); }, out);
}

// ---- JSON: AnimTreeNode tree ----

void to_json(nlohmann::json& j, const AnimTreeNode& node) {
	j["name"] = node.name;
	j["kind"] = get_kind_display_name(node.kind);
	nlohmann::json params_j;
	params_to_json(params_j, node.kind, node.params);
	j["params"] = params_j;
	auto& arr = j["children"] = nlohmann::json::array();
	for (auto& c : node.children) {
		nlohmann::json cj;
		to_json(cj, *c);
		arr.push_back(std::move(cj));
	}
}
void from_json(const nlohmann::json& j, AnimTreeNode& node) {
	node.name = j.value("name", std::string());
	node.kind = kind_from_string(j.value("kind", std::string("Clip")));
	if (j.contains("params"))
		params_from_json(j.at("params"), node.kind, node.params);
	else
		node.params = make_default_params_for_kind(node.kind);
	node.children.clear();
	if (j.contains("children")) {
		for (auto& cj : j.at("children")) {
			auto child = std::make_unique<AnimTreeNode>();
			from_json(cj, *child);
			node.children.push_back(std::move(child));
		}
	}
}

// ---- IAsset ----

bool AnimTreeAsset::load_asset() {
	auto file = FileSys::open_read_game(get_name());
	if (!file) {
		sys_print(Warning, "AnimTreeAsset: failed to open %s\n", get_name().c_str());
		return false;
	}
	size_t file_size = file->size();
	std::string text;
	text.resize(file_size);
	file->read(text.data(), file_size);
	file->close();

	try {
		auto j = nlohmann::json::parse(text);
		std::string skel_path = j.value("skeleton", std::string());
		skeleton = skel_path.empty() ? AssetPtr<Model>{} : g_assets.find<Model>(skel_path);

		root = std::make_unique<AnimTreeNode>();
		if (j.contains("root"))
			from_json(j.at("root"), *root);

		cached_pose_roots.clear();
		if (j.contains("cached_pose_roots")) {
			for (auto& cpj : j.at("cached_pose_roots")) {
				auto node = std::make_unique<AnimTreeNode>();
				from_json(cpj.at("root"), *node);
				cached_pose_roots.emplace_back(cpj.value("name", std::string()), std::move(node));
			}
		}
	}
	catch (const nlohmann::json::exception& e) {
		sys_print(Warning, "AnimTreeAsset: JSON parse error in %s: %s\n", get_name().c_str(), e.what());
		return false;
	}
	return true;
}

void AnimTreeAsset::post_load() {}

void AnimTreeAsset::uninstall() {
	root = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
	cached_pose_roots.clear();
	skeleton = AssetPtr<Model>{};
}

void AnimTreeAsset::save_to_disk() {
	nlohmann::json j;
	j["skeleton"] = skeleton.get() ? skeleton.get()->get_name() : "";
	nlohmann::json root_j;
	to_json(root_j, *root);
	j["root"] = root_j;
	auto& arr = j["cached_pose_roots"] = nlohmann::json::array();
	for (auto& entry : cached_pose_roots) {
		nlohmann::json cpj;
		cpj["name"] = entry.first;
		nlohmann::json rj;
		to_json(rj, *entry.second);
		cpj["root"] = rj;
		arr.push_back(std::move(cpj));
	}

	std::string text = j.dump(2);
	auto file = FileSys::open_write_game(get_name());
	if (!file) {
		sys_print(Warning, "AnimTreeAsset: failed to write %s\n", get_name().c_str());
		return;
	}
	file->write(text.data(), text.size());
	file->close();
}

REF AnimTreeAsset* AnimTreeAsset::load(const std::string& name) {
	return g_assets.find<AnimTreeAsset>(name).get();
}
