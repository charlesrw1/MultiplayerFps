#include <gtest/gtest.h>
#include "Game/Components/RagdollUtil.h"

TEST(RagdollUtilTest, RightSuffixDetected) {
	EXPECT_TRUE(ragdoll_is_right_side("upperarm_r"));
	EXPECT_TRUE(ragdoll_is_left_side("upperarm_l"));
	EXPECT_FALSE(ragdoll_is_right_side("upperarm_l"));
	EXPECT_FALSE(ragdoll_is_left_side("upperarm_r"));
}

TEST(RagdollUtilTest, SuffixMustBeTrailing) {
	// Unreal semantics: only a trailing "_r"/"_l" counts, not "contains right/left anywhere".
	EXPECT_FALSE(ragdoll_is_right_side("right_upperarm"));
	EXPECT_FALSE(ragdoll_is_left_side("left_upperarm"));
	EXPECT_FALSE(ragdoll_is_right_side("upperarm_right"));
}

TEST(RagdollUtilTest, CenterBoneIsNeitherSide) {
	EXPECT_FALSE(ragdoll_is_right_side("spine_02"));
	EXPECT_FALSE(ragdoll_is_left_side("spine_02"));
}

TEST(RagdollUtilTest, MirrorSwapsTrailingSuffix) {
	EXPECT_EQ(ragdoll_mirror_bone_name("upperarm_r"), "upperarm_l");
	EXPECT_EQ(ragdoll_mirror_bone_name("upperarm_l"), "upperarm_r");
	EXPECT_EQ(ragdoll_mirror_bone_name("hand_R"), "hand_L");
	EXPECT_EQ(ragdoll_mirror_bone_name("hand_L"), "hand_R");
}

TEST(RagdollUtilTest, MirrorLeavesUnsidedNamesUnchanged) {
	EXPECT_EQ(ragdoll_mirror_bone_name("spine_02"), "spine_02");
	EXPECT_EQ(ragdoll_mirror_bone_name("pelvis"), "pelvis");
}

TEST(RagdollUtilTest, MirrorIsSymmetric) {
	std::string right = "clavicle_r";
	std::string mirrored = ragdoll_mirror_bone_name(right);
	EXPECT_EQ(ragdoll_mirror_bone_name(mirrored), right);
}

TEST(RagdollUtilTest, ToLowerLowercasesAscii) {
	EXPECT_EQ(ragdoll_str_to_lower("UpperArm_R"), "upperarm_r");
}
