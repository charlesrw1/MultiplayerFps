#pragma once
#include <string>
#include <cctype>

// Unreal Engine bone-naming convention: side is encoded as a trailing "_l"/"_r" suffix
// (case-insensitive), e.g. "upperarm_r", "hand_L". Mirroring only ever swaps that suffix --
// no "contains left/right anywhere" fallback.

inline std::string ragdoll_str_to_lower(std::string s) {
	for (auto& c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

inline bool ragdoll_is_right_side(const std::string& lower) {
	return lower.size() >= 2 && lower.compare(lower.size() - 2, 2, "_r") == 0;
}
inline bool ragdoll_is_left_side(const std::string& lower) {
	return lower.size() >= 2 && lower.compare(lower.size() - 2, 2, "_l") == 0;
}

// Swaps a trailing _r/_l (or _R/_L) suffix, preserving the rest of the name and the suffix's
// original case. Returns the name unchanged if it has no recognized side suffix (caller must
// check ragdoll_is_right_side/ragdoll_is_left_side first if it needs to know whether this
// actually mirrored anything).
inline std::string ragdoll_mirror_bone_name(const std::string& bone_name) {
	if (bone_name.size() < 2 || bone_name[bone_name.size() - 2] != '_')
		return bone_name;
	char last = bone_name[bone_name.size() - 1];
	std::string prefix = bone_name.substr(0, bone_name.size() - 1);
	switch (last) {
	case 'r': return prefix + "l";
	case 'R': return prefix + "L";
	case 'l': return prefix + "r";
	case 'L': return prefix + "R";
	default: return bone_name;
	}
}
