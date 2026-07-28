#include <gtest/gtest.h>
#include "Animation/AnimTreeAsset.h"
#include "Animation/AnimTreeBuild.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"

// Builds the example tree from the AnimTree design doc:
//   root -> IK(foot_l) -> IK(foot_r) -> Slot(full_body_slot) -> BlendByInt(i_my_state_var)
//                                                                    Add(f_additive_var)
//                                                                        Clip(running_clip)
//                                                                        Clip(walking_clip)
//                                                                    Clip(idle_clip)
//                                                                    Slot(lower_body_slot)
static std::unique_ptr<AnimTreeNode> make_sample_tree() {
	auto clip = [](const char* name, const char* clip_name) {
		auto n = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::Clip);
		n->name = name;
		auto& p = std::get<AnimTreeClipParams>(n->params);
		p.sync_group = StringName(clip_name);
		return n;
	};

	auto add = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::Add);
	add->name = "add_node";
	std::get<AnimTreeAddParams>(add->params).alpha.mode = AnimTreeValueMode::Variable;
	std::get<AnimTreeAddParams>(add->params).alpha.var_name = StringName("f_additive_var");
	add->children.push_back(clip("running_clip_node", "running_clip_N"));
	add->children.push_back(clip("walking_clip_node", "walking_clip_N"));

	auto blend_by_int = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BlendByInt);
	blend_by_int->name = "state_switch";
	auto& bbi = std::get<AnimTreeBlendByIntParams>(blend_by_int->params);
	bbi.integer.mode = AnimTreeValueMode::Variable;
	bbi.integer.var_name = StringName("i_my_state_var");
	blend_by_int->children.push_back(std::move(add));
	blend_by_int->children.push_back(clip("idle_clip_node", "idle_clip"));
	auto lower_slot = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::Slot);
	lower_slot->name = "lower_body_slot_node";
	std::get<AnimTreeSlotParams>(lower_slot->params).slot_name = StringName("lower_body_slot");
	blend_by_int->children.push_back(std::move(lower_slot));

	auto full_slot = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::Slot);
	full_slot->name = "full_body_slot_node";
	std::get<AnimTreeSlotParams>(full_slot->params).slot_name = StringName("full_body_slot");
	full_slot->children.push_back(std::move(blend_by_int));

	auto ik_r = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::IK);
	ik_r->name = "ik_foot_r";
	std::get<AnimTreeIkParams>(ik_r->params).bone_name = StringName("foot_r");
	ik_r->children.push_back(std::move(full_slot));

	auto ik_l = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::IK);
	ik_l->name = "ik_foot_l";
	std::get<AnimTreeIkParams>(ik_l->params).bone_name = StringName("foot_l");
	ik_l->children.push_back(std::move(ik_r));

	return ik_l;
}

static bool str_eq(const StringName& a, const StringName& b) { return a == b; }

static void expect_nodes_equal(const AnimTreeNode& a, const AnimTreeNode& b) {
	EXPECT_EQ(a.name, b.name);
	EXPECT_EQ((int)a.kind, (int)b.kind);
	ASSERT_EQ(a.children.size(), b.children.size());
	for (size_t i = 0; i < a.children.size(); i++)
		expect_nodes_equal(*a.children[i], *b.children[i]);
}

TEST(AnimTreeJson, RoundTripPreservesShapeAndNames) {
	auto original = make_sample_tree();

	nlohmann::json j;
	to_json(j, *original);

	AnimTreeNode reloaded;
	from_json(j, reloaded);

	expect_nodes_equal(*original, reloaded);

	// Spot-check a couple of params survived the round trip.
	ASSERT_EQ(reloaded.kind, AnimTreeNodeKind::IK);
	EXPECT_TRUE(str_eq(std::get<AnimTreeIkParams>(reloaded.params).bone_name, StringName("foot_l")));

	auto& state_switch = *reloaded.children[0]->children[0]->children[0];
	ASSERT_EQ(state_switch.kind, AnimTreeNodeKind::BlendByInt);
	auto& bbi = std::get<AnimTreeBlendByIntParams>(state_switch.params);
	EXPECT_EQ(bbi.integer.mode, AnimTreeValueMode::Variable);
	EXPECT_TRUE(str_eq(bbi.integer.var_name, StringName("i_my_state_var")));
}

TEST(AnimTreeBuild, GraphShapeAndNamedNodesMatchSourceTree) {
	AnimTreeAsset asset;
	asset.root = make_sample_tree();

	agBuilder builder;
	auto result = AnimTreeBuild::build(asset, builder);

	ASSERT_NE(result.root, nullptr);
	auto* ik_l = dynamic_cast<agIk2Bone*>(result.root);
	ASSERT_NE(ik_l, nullptr);
	EXPECT_TRUE(ik_l->bone_name == StringName("foot_l"));

	auto* ik_r = dynamic_cast<agIk2Bone*>(ik_l->input);
	ASSERT_NE(ik_r, nullptr);
	EXPECT_TRUE(ik_r->bone_name == StringName("foot_r"));

	auto* full_slot = dynamic_cast<agSlotPlayer*>(ik_r->input);
	ASSERT_NE(full_slot, nullptr);

	auto* state_switch = dynamic_cast<agBlendByInt*>(full_slot->input);
	ASSERT_NE(state_switch, nullptr);
	ASSERT_EQ(state_switch->inputs.size(), 3u);

	auto* add_node = dynamic_cast<agAddNode*>(state_switch->inputs[0]);
	ASSERT_NE(add_node, nullptr);
	ASSERT_NE(add_node->input0, nullptr);
	ASSERT_NE(add_node->input1, nullptr);
	EXPECT_NE(dynamic_cast<agClipNode*>(add_node->input0), nullptr);
	EXPECT_NE(dynamic_cast<agClipNode*>(add_node->input1), nullptr);

	EXPECT_NE(dynamic_cast<agClipNode*>(state_switch->inputs[1]), nullptr);
	auto* lower_slot = dynamic_cast<agSlotPlayer*>(state_switch->inputs[2]);
	ASSERT_NE(lower_slot, nullptr);

	// named_nodes should contain every named node from the source tree, resolving to the
	// matching runtime node.
	EXPECT_EQ(result.named_nodes.at("ik_foot_l"), (agBaseNode*)ik_l);
	EXPECT_EQ(result.named_nodes.at("ik_foot_r"), (agBaseNode*)ik_r);
	EXPECT_EQ(result.named_nodes.at("full_body_slot_node"), (agBaseNode*)full_slot);
	EXPECT_EQ(result.named_nodes.at("state_switch"), (agBaseNode*)state_switch);
	EXPECT_EQ(result.named_nodes.at("add_node"), (agBaseNode*)add_node);
	EXPECT_EQ(result.named_nodes.at("lower_body_slot_node"), (agBaseNode*)lower_slot);
	// ik_foot_l, ik_foot_r, full_body_slot_node, state_switch, add_node, running_clip_node,
	// walking_clip_node, idle_clip_node, lower_body_slot_node
	EXPECT_EQ(result.named_nodes.size(), 9u);

	for (auto* n : builder.get_all_nodes())
		delete n;
}
