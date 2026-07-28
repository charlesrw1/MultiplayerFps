#include "AnimTreeBuild.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Render/Model.h"

namespace {

ValueType to_value_type(const AnimTreeFloatValue& v) { return v.mode == AnimTreeValueMode::Variable ? ValueType(v.var_name) : ValueType(v.constant); }
ValueType to_value_type(const AnimTreeIntValue& v) { return v.mode == AnimTreeValueMode::Variable ? ValueType(v.var_name) : ValueType(v.constant); }
ValueType to_value_type(const AnimTreeBoolValue& v) { return v.mode == AnimTreeValueMode::Variable ? ValueType(v.var_name) : ValueType(v.constant); }
ValueType to_value_type(const AnimTreeVec3Value& v) { return v.mode == AnimTreeValueMode::Variable ? ValueType(v.var_name) : ValueType(v.constant); }

// Applies one authored bone mask (weighted or binary) onto a runtime node, in list order --
// exactly the replay order the BoneMaskEditor's effective-weight preview uses, so the two never
// drift (see Editor/BoneMaskEditor.cpp).
void apply_blend_mask(agBlendMasked& node, const AnimTreeBlendMaskedParams& params, const Model* model) {
	if (!model || !model->get_skel())
		return;
	node.init_mask_for_model(model, params.default_weight);
	for (auto& entry : params.mask) {
		std::string bone = entry.bone.get_c_str() ? entry.bone.get_c_str() : "";
		if (bone.empty())
			continue;
		if (entry.include_children)
			node.set_all_children_weights(model, bone, entry.weight);
		else
			node.set_one_bone_weight(model, bone, entry.weight);
	}
}

void apply_additive_mask(agMakeAdditive& node, const AnimTreeMakeAdditiveParams& params, const Model* model) {
	if (!model || !model->get_skel())
		return;
	node.init_mask(model);
	for (auto& bone : params.masked_bones) {
		std::string name = bone.get_c_str() ? bone.get_c_str() : "";
		if (!name.empty())
			node.mask_bone_and_children(model, name);
	}
}

// Recursively allocates `node` and its children into `builder`, post-order (children built
// before their parent so the parent can wire them straight in).
agBaseNode* build_node(const AnimTreeNode& node, agBuilder& builder, const Model* model, AnimTreeBuild::BuildResult& out) {
	// Build children first.
	std::vector<agBaseNode*> children;
	children.reserve(node.children.size());
	for (auto& c : node.children)
		children.push_back(build_node(*c, builder, model, out));

	agBaseNode* result = nullptr;

	switch (node.kind) {
	case AnimTreeNodeKind::Clip: {
		auto& p = std::get<AnimTreeClipParams>(node.params);
		auto* n = builder.alloc<agClipNode>();
		if (p.clip.get())
			n->set_clip(p.clip.get());
		n->set_looping(p.looping);
		n->speed = to_value_type(p.speed);
		n->syncGroup = p.sync_group;
		n->syncType = p.sync_type;
		result = n;
		break;
	}
	case AnimTreeNodeKind::BindPose: {
		result = builder.alloc<agBindPose>();
		break;
	}
	case AnimTreeNodeKind::Blend: {
		auto& p = std::get<AnimTreeBlendParams>(node.params);
		auto* n = builder.alloc<agBlendNode>();
		n->input0 = children.size() > 0 ? children[0] : nullptr;
		n->input1 = children.size() > 1 ? children[1] : nullptr;
		n->alpha = to_value_type(p.alpha);
		result = n;
		break;
	}
	case AnimTreeNodeKind::BlendMasked: {
		auto& p = std::get<AnimTreeBlendMaskedParams>(node.params);
		auto* n = builder.alloc<agBlendMasked>();
		n->input0 = children.size() > 0 ? children[0] : nullptr;
		n->input1 = children.size() > 1 ? children[1] : nullptr;
		n->alpha = to_value_type(p.alpha);
		n->meshspace_blend = p.meshspace_blend;
		apply_blend_mask(*n, p, model);
		result = n;
		break;
	}
	case AnimTreeNodeKind::Add: {
		auto& p = std::get<AnimTreeAddParams>(node.params);
		auto* n = builder.alloc<agAddNode>();
		n->input0 = children.size() > 0 ? children[0] : nullptr;
		n->input1 = children.size() > 1 ? children[1] : nullptr;
		n->alpha = to_value_type(p.alpha);
		result = n;
		break;
	}
	case AnimTreeNodeKind::MakeAdditive: {
		auto& p = std::get<AnimTreeMakeAdditiveParams>(node.params);
		auto* n = builder.alloc<agMakeAdditive>();
		n->input = children.size() > 0 ? children[0] : nullptr;
		n->reference = children.size() > 1 ? children[1] : nullptr;
		apply_additive_mask(*n, p, model);
		result = n;
		break;
	}
	case AnimTreeNodeKind::IK: {
		auto& p = std::get<AnimTreeIkParams>(node.params);
		auto* n = builder.alloc<agIk2Bone>();
		n->input = children.size() > 0 ? children[0] : nullptr;
		n->bone_name = p.bone_name;
		n->other_bone = p.other_bone;
		n->take_rotation_of_other = p.take_rotation_of_other;
		n->ik_in_bone_space = p.ik_in_bone_space;
		n->pole_bone = p.pole_bone;
		n->pole_in_bone_space = p.pole_in_bone_space;
		n->allow_stretching = p.allow_stretching;
		n->max_stretch_scale = p.max_stretch_scale;
		n->start_stretch_ratio = p.start_stretch_ratio;
		n->alpha = to_value_type(p.alpha);
		n->target = to_value_type(p.target);
		n->pole = to_value_type(p.pole);
		result = n;
		break;
	}
	case AnimTreeNodeKind::ModifyBone: {
		auto& p = std::get<AnimTreeModifyBoneParams>(node.params);
		auto* n = builder.alloc<agModifyBone>();
		n->input = children.size() > 0 ? children[0] : nullptr;
		n->boneName = p.bone_name;
		n->translation = p.translation_mode;
		n->rotation = p.rotation_mode;
		n->scale = p.scale_mode;
		n->translationVal = to_value_type(p.translation);
		n->rotationVal = to_value_type(p.rotation);
		n->scaleVal = to_value_type(p.scale);
		n->alpha = to_value_type(p.alpha);
		result = n;
		break;
	}
	case AnimTreeNodeKind::CopyBone: {
		auto& p = std::get<AnimTreeCopyBoneParams>(node.params);
		auto* n = builder.alloc<agCopyBone>();
		n->input = children.size() > 0 ? children[0] : nullptr;
		n->sourceBone = p.source_bone;
		n->targetBone = p.target_bone;
		n->copyTranslation = to_value_type(p.copy_translation);
		n->copyRotation = to_value_type(p.copy_rotation);
		n->copyScale = to_value_type(p.copy_scale);
		n->copyBonespace = p.copy_bonespace;
		n->alpha = to_value_type(p.alpha);
		result = n;
		break;
	}
	case AnimTreeNodeKind::Slot: {
		auto& p = std::get<AnimTreeSlotParams>(node.params);
		auto* n = builder.alloc<agSlotPlayer>();
		n->initialize(p.slot_name, p.update_children_when_playing, children.size() > 0 ? children[0] : nullptr);
		builder.add_slot_name(p.slot_name);
		result = n;
		break;
	}
	case AnimTreeNodeKind::BlendByInt: {
		auto& p = std::get<AnimTreeBlendByIntParams>(node.params);
		auto* n = builder.alloc<agBlendByInt>();
		n->set_transition_data(p.easing, p.blend_duration);
		n->integer = to_value_type(p.integer);
		for (auto* c : children)
			n->append_input(c);
		result = n;
		break;
	}
	case AnimTreeNodeKind::SaveCachedPose: {
		auto& p = std::get<AnimTreeSaveCachedPoseParams>(node.params);
		auto* n = builder.alloc<agSaveCachedPose>();
		n->input = children.size() > 0 ? children[0] : nullptr;
		n->set_cache_name(p.cache_name);
		builder.add_cached_pose_root(n);
		result = n;
		break;
	}
	case AnimTreeNodeKind::UseCachedPose: {
		auto& p = std::get<AnimTreeUseCachedPoseParams>(node.params);
		auto* n = builder.alloc<agUseCachedPose>();
		n->set_cache_name(p.cache_name);
		result = n;
		break;
	}
	}

	if (result && !node.name.empty())
		out.named_nodes[node.name] = result;

	return result;
}

} // namespace

namespace AnimTreeBuild
{
	BuildResult build(const AnimTreeAsset& asset, agBuilder& builder) {
		BuildResult out;
		const Model* model = asset.skeleton.get();

		if (asset.root)
			out.root = build_node(*asset.root, builder, model, out);
		builder.set_root(out.root);

		for (auto& entry : asset.cached_pose_roots) {
			if (!entry.second)
				continue;
			agBaseNode* subtree = build_node(*entry.second, builder, model, out);
			auto* wrapper = builder.alloc<agSaveCachedPose>();
			wrapper->input = subtree;
			wrapper->set_cache_name(StringName(entry.first.c_str()));
			builder.add_cached_pose_root(wrapper);
		}

		return out;
	}
}
