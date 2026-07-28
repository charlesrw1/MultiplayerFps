#pragma once
#ifdef EDITOR_BUILD
#include "Animation/AnimTreeAsset.h"
#include <string>
#include <vector>

class MSkeleton;

// Skeleton-aware bone weight-paint widget for BlendMasked (weighted) and MakeAdditive (binary),
// modeled on Unreal's blend mask / blend profile editors: paint a weight (or on/off) onto a
// bone, optionally propagating to everything below it in the hierarchy. See AnimTreeAsset.h for
// BoneMaskEntry and AnimTreeBuild.cpp for how the authored list is replayed onto the runtime
// node -- this widget's "effective weight" preview follows that exact same replay order so the
// two never disagree about what a bone's weight actually is.
class BoneMaskEditor
{
public:
	// Returns true if `mask` or `default_weight` changed this frame.
	bool draw(const MSkeleton& skel, std::vector<BoneMaskEntry>& mask, float& default_weight);
	// Binary variant for MakeAdditive::masked_bones (weight is implicitly 0/1, no propagation toggle --
	// masking is always bone+descendants, matching agMakeAdditive::mask_bone_and_children).
	bool draw_binary(const MSkeleton& skel, std::vector<StringName>& masked_bones);

private:
	void rebuild_hierarchy_if_needed(const MSkeleton& skel);
	void recompute_effective_weights(const std::vector<BoneMaskEntry>& mask, float default_weight);
	bool draw_bone_row_weighted(const MSkeleton& skel, std::vector<BoneMaskEntry>& mask, int bone_idx, bool& changed);
	bool draw_bone_row_binary(const MSkeleton& skel, std::vector<StringName>& masked_bones, int bone_idx, bool& changed);
	bool bone_or_descendant_matches_filter(const MSkeleton& skel, int bone_idx) const;

	const MSkeleton* cached_skel_ = nullptr;
	std::vector<std::vector<int>> children_;
	std::vector<int> roots_;
	std::vector<float> effective_weight_; // parallel to skeleton bones
	std::vector<int> explicit_entry_index_; // parallel to skeleton bones; -1 if no explicit BoneMaskEntry

	char filter_buf_[128] = {};
};
#endif
