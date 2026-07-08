#ifdef EDITOR_BUILD
#include "Assets/AssetReferenceViewer.h"
#include "Assets/AssetRegistry.h"
#include "imgui.h"
#include <algorithm>

AssetReferenceViewer& AssetReferenceViewer::get() {
	static AssetReferenceViewer inst;
	return inst;
}

static bool contains_ci(const std::string& haystack, const char* needle) {
	if (!needle[0])
		return true;
	for (const char* h = haystack.c_str(); *h; ++h) {
		const char* a = h;
		const char* b = needle;
		while (*a && *b && tolower(*a) == tolower(*b)) { ++a; ++b; }
		if (!*b) return true;
	}
	return false;
}

void AssetReferenceViewer::open_for(const std::string& asset_gamepath) {
	current_asset = asset_gamepath;
	back_stack.clear();
	forward_stack.clear();
	dirty = true;
	is_open = true;
}

void AssetReferenceViewer::navigate_to(const std::string& gamepath) {
	if (gamepath == current_asset)
		return;
	back_stack.push_back(current_asset);
	forward_stack.clear();
	current_asset = gamepath;
	dirty = true;
}

void AssetReferenceViewer::run_query() {
	if (transitive)
		results = AssetReferenceQuery::find_transitive_references(current_asset, direction_backward);
	else if (direction_backward)
		results = AssetReferenceQuery::find_backward_references(current_asset);
	else
		results = AssetReferenceQuery::find_forward_references(current_asset);

	std::sort(results.begin(), results.end(), [](const AssetRefHit& a, const AssetRefHit& b) {
		return a.game_path < b.game_path;
	});
	dirty = false;
}

void AssetReferenceViewer::imgui_draw() {
	if (!is_open)
		return;

	ImGui::SetNextWindowSize({700, 500}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Asset References", &is_open)) {
		ImGui::End();
		return;
	}

	if (current_asset.empty()) {
		ImGui::TextDisabled("No asset selected.");
		ImGui::End();
		return;
	}

	// --- Navigation row ---
	ImGui::BeginDisabled(back_stack.empty());
	if (ImGui::Button("<")) {
		forward_stack.push_back(current_asset);
		current_asset = back_stack.back();
		back_stack.pop_back();
		dirty = true;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(forward_stack.empty());
	if (ImGui::Button(">")) {
		back_stack.push_back(current_asset);
		current_asset = forward_stack.back();
		forward_stack.pop_back();
		dirty = true;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::SetWindowFontScale(1.3f);
	ImGui::TextUnformatted(current_asset.c_str());
	ImGui::SetWindowFontScale(1.0f);

	// --- Filter row ---
	if (ImGui::RadioButton("Referenced By", direction_backward)) { direction_backward = true; dirty = true; }
	ImGui::SameLine();
	if (ImGui::RadioButton("References", !direction_backward)) { direction_backward = false; dirty = true; }
	ImGui::SameLine();
	if (ImGui::Checkbox("Transitive", &transitive))
		dirty = true;
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh"))
		dirty = true;

	if (ImGui::SmallButton("Type filters..."))
		ImGui::OpenPopup("type_popup_refs");
	if (ImGui::BeginPopup("type_popup_refs")) {
		bool is_hiding_all = type_filter_mask != 0;
		if (ImGui::Checkbox("Show/Hide all", &is_hiding_all))
			type_filter_mask = (type_filter_mask != 0) ? 0 : ~0u;
		for (auto& type : AssetRegistrySystem::get().get_types())
			ImGui::CheckboxFlags(type->get_type_name().c_str(), &type_filter_mask, 1u << type->self_index);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##ref_text_filter", "Filter...", text_filter, sizeof(text_filter));

	ImGui::Separator();

	if (dirty)
		run_query();

	int shown = 0;
	if (ImGui::BeginTable("RefTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120);
		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableHeadersRow();

		for (auto& hit : results) {
			if (hit.type && !(type_filter_mask & (1u << hit.type->self_index)))
				continue;
			if (text_filter[0] && !contains_ci(hit.game_path, text_filter))
				continue;
			shown++;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(&hit);
			if (ImGui::Selectable(hit.game_path.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
				navigate_to(hit.game_path);
			ImGui::PopID();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(hit.type ? hit.type->get_type_name().c_str() : "?");
			ImGui::TableNextColumn();
			ImGui::Text("x%d", hit.count);
		}

		ImGui::EndTable();
	}
	ImGui::Text("%d of %zu shown", shown, results.size());

	ImGui::End();
}

#endif
