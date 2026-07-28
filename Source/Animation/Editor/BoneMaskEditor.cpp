#ifdef EDITOR_BUILD
#include "BoneMaskEditor.h"
#include "Animation/SkeletonData.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

void BoneMaskEditor::rebuild_hierarchy_if_needed(const MSkeleton& skel) {
	if (cached_skel_ == &skel)
		return;
	cached_skel_ = &skel;
	const int n = skel.get_num_bones();
	children_.assign(n, {});
	roots_.clear();
	for (int i = 0; i < n; i++) {
		int p = skel.get_bone_parent(i);
		if (p >= 0 && p < n && p != i)
			children_[p].push_back(i);
		else
			roots_.push_back(i);
	}
}

void BoneMaskEditor::recompute_effective_weights(const std::vector<BoneMaskEntry>& mask, float default_weight) {
	const int n = (int)children_.size();
	effective_weight_.assign(n, default_weight);
	explicit_entry_index_.assign(n, -1);
	if (!cached_skel_)
		return;

	// Replay authored entries in order -- must match AnimTreeBuild.cpp's apply_blend_mask exactly.
	for (int ei = 0; ei < (int)mask.size(); ei++) {
		const auto& entry = mask[ei];
		int idx = cached_skel_->get_bone_index(entry.bone);
		if (idx < 0 || idx >= n)
			continue;
		explicit_entry_index_[idx] = ei;
		if (entry.include_children) {
			std::vector<int> stack = {idx};
			while (!stack.empty()) {
				int b = stack.back();
				stack.pop_back();
				effective_weight_[b] = entry.weight;
				for (int c : children_[b])
					stack.push_back(c);
			}
		} else {
			effective_weight_[idx] = entry.weight;
		}
	}
}

static ImVec4 weight_color(float w) {
	w = std::clamp(w, 0.f, 1.f);
	const ImVec4 cool(0.35f, 0.42f, 0.58f, 1.f); // weight 0
	const ImVec4 warm(0.95f, 0.55f, 0.15f, 1.f); // weight 1
	return ImVec4(cool.x + (warm.x - cool.x) * w, cool.y + (warm.y - cool.y) * w,
				  cool.z + (warm.z - cool.z) * w, 1.f);
}

bool BoneMaskEditor::bone_or_descendant_matches_filter(const MSkeleton& skel, int bone_idx) const {
	if (filter_buf_[0] == 0)
		return true;
	const std::string& name = skel.get_all_bones()[bone_idx].strname;
	std::string lower_name = name, lower_filter = filter_buf_;
	std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
	std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
	if (lower_name.find(lower_filter) != std::string::npos)
		return true;
	for (int c : children_[bone_idx])
		if (bone_or_descendant_matches_filter(skel, c))
			return true;
	return false;
}

static int find_entry(std::vector<BoneMaskEntry>& mask, StringName bone) {
	for (int i = 0; i < (int)mask.size(); i++)
		if (mask[i].bone == bone)
			return i;
	return -1;
}

bool BoneMaskEditor::draw_bone_row_weighted(const MSkeleton& skel, std::vector<BoneMaskEntry>& mask, int bone_idx, bool& changed) {
	if (!bone_or_descendant_matches_filter(skel, bone_idx))
		return false;

	const auto& bones = skel.get_all_bones();
	const bool has_explicit = explicit_entry_index_[bone_idx] >= 0;
	const float w = effective_weight_[bone_idx];

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (children_[bone_idx].empty())
		flags |= ImGuiTreeNodeFlags_Leaf;

	ImGui::PushID(bone_idx);
	if (has_explicit)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
	else
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.65f, 1.f));
	bool open = ImGui::TreeNodeEx(bones[bone_idx].strname.c_str(), flags);
	ImGui::PopStyleColor();

	if (has_explicit) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.2f, 1.f), "*");
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x > 220.f ? ImGui::GetWindowWidth() - 220.f : 160.f);
	ImGui::SetNextItemWidth(120.f);
	float slider_val = w;
	ImVec4 col = weight_color(w);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, col);
	ImGui::PushStyleColor(ImGuiCol_SliderGrab, col);
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col);
	if (ImGui::SliderFloat("##w", &slider_val, 0.f, 1.f, "%.2f")) {
		int ei = find_entry(mask, bones[bone_idx].name);
		if (ei < 0) {
			BoneMaskEntry e;
			e.bone = bones[bone_idx].name;
			e.weight = slider_val;
			e.include_children = true;
			mask.push_back(e);
		} else {
			mask[ei].weight = slider_val;
			// Move to end so it wins over any earlier overlapping entry (last-write-wins).
			BoneMaskEntry moved = mask[ei];
			mask.erase(mask.begin() + ei);
			mask.push_back(moved);
		}
		changed = true;
	}
	ImGui::PopStyleColor(3);

	if (has_explicit) {
		ImGui::SameLine();
		bool include_children = mask[explicit_entry_index_[bone_idx]].include_children;
		if (ImGui::SmallButton(include_children ? "\xF0\x9F\x94\x97" /* chain-ish */ : "1")) {
			mask[explicit_entry_index_[bone_idx]].include_children = !include_children;
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(include_children ? "Applies to this bone + all children (click to change)" : "Applies to this bone only (click to change)");
	}

	if (ImGui::BeginPopupContextItem("##bone_ctx")) {
		if (ImGui::MenuItem("Clear Override", nullptr, false, has_explicit)) {
			mask.erase(mask.begin() + explicit_entry_index_[bone_idx]);
			changed = true;
		}
		auto quick_set = [&](float v) {
			int ei = find_entry(mask, bones[bone_idx].name);
			if (ei < 0) {
				BoneMaskEntry e; e.bone = bones[bone_idx].name; e.weight = v; e.include_children = true;
				mask.push_back(e);
			} else {
				mask[ei].weight = v;
			}
			changed = true;
		};
		if (ImGui::MenuItem("Set 0 (+ children)")) quick_set(0.f);
		if (ImGui::MenuItem("Set 0.5 (+ children)")) quick_set(0.5f);
		if (ImGui::MenuItem("Set 1 (+ children)")) quick_set(1.f);
		ImGui::EndPopup();
	}

	if (open) {
		for (int c : children_[bone_idx])
			draw_bone_row_weighted(skel, mask, c, changed);
		ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

bool BoneMaskEditor::draw_bone_row_binary(const MSkeleton& skel, std::vector<StringName>& masked_bones, int bone_idx, bool& changed) {
	if (!bone_or_descendant_matches_filter(skel, bone_idx))
		return false;

	const auto& bones = skel.get_all_bones();
	bool is_masked = false;
	for (auto& b : masked_bones)
		if (b == bones[bone_idx].name) { is_masked = true; break; }

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (children_[bone_idx].empty())
		flags |= ImGuiTreeNodeFlags_Leaf;

	ImGui::PushID(bone_idx);
	ImGui::PushStyleColor(ImGuiCol_Text, is_masked ? ImVec4(0.95f, 0.55f, 0.15f, 1.f) : ImVec4(0.75f, 0.75f, 0.75f, 1.f));
	bool open = ImGui::TreeNodeEx(bones[bone_idx].strname.c_str(), flags);
	ImGui::PopStyleColor();

	ImGui::SameLine(ImGui::GetWindowWidth() - 60.f);
	bool checked = is_masked;
	if (ImGui::Checkbox("##masked", &checked)) {
		if (checked)
			masked_bones.push_back(bones[bone_idx].name);
		else
			masked_bones.erase(std::remove(masked_bones.begin(), masked_bones.end(), bones[bone_idx].name), masked_bones.end());
		changed = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Zero this bone's additive delta (+ all children)");

	if (open) {
		for (int c : children_[bone_idx])
			draw_bone_row_binary(skel, masked_bones, c, changed);
		ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

bool BoneMaskEditor::draw(const MSkeleton& skel, std::vector<BoneMaskEntry>& mask, float& default_weight) {
	rebuild_hierarchy_if_needed(skel);
	bool changed = false;

	ImGui::PushID(this);
	ImGui::SetNextItemWidth(160.f);
	ImGui::InputTextWithHint("##filter", "Search bones...", filter_buf_, sizeof(filter_buf_));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.f);
	if (ImGui::DragFloat("Default Weight", &default_weight, 0.01f, 0.f, 1.f))
		changed = true;
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear All Overrides")) {
		mask.clear();
		changed = true;
	}

	recompute_effective_weights(mask, default_weight);

	ImGui::BeginChild("##bone_mask_tree", ImVec2(0, 260), true);
	for (int r : roots_)
		draw_bone_row_weighted(skel, mask, r, changed);
	ImGui::EndChild();
	ImGui::PopID();

	if (changed)
		recompute_effective_weights(mask, default_weight);
	return changed;
}

bool BoneMaskEditor::draw_binary(const MSkeleton& skel, std::vector<StringName>& masked_bones) {
	rebuild_hierarchy_if_needed(skel);
	bool changed = false;

	ImGui::PushID(this);
	ImGui::SetNextItemWidth(160.f);
	ImGui::InputTextWithHint("##filter", "Search bones...", filter_buf_, sizeof(filter_buf_));
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear All")) {
		masked_bones.clear();
		changed = true;
	}

	ImGui::BeginChild("##bone_mask_tree_binary", ImVec2(0, 260), true);
	for (int r : roots_)
		draw_bone_row_binary(skel, masked_bones, r, changed);
	ImGui::EndChild();
	ImGui::PopID();
	return changed;
}
#endif
