#ifdef EDITOR_BUILD
#include "AssetTools/DiagnosticsWindow.h"
#include "AssetTools/AssetDiagnostics.h"
#include "imgui.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Framework/StringUtils.h"
#include <algorithm>
#include <unordered_map>

DiagnosticsWindow& DiagnosticsWindow::get() {
	static DiagnosticsWindow inst;
	return inst;
}

static bool contains_ci(const char* haystack, const char* needle) {
	if (!needle[0]) return true;
	for (const char* h = haystack; *h; ++h) {
		const char* a = h;
		const char* b = needle;
		while (*a && *b && tolower(*a) == tolower(*b)) { ++a; ++b; }
		if (!*b) return true;
	}
	return false;
}

static ImVec4 severity_color(AssetSeverity s) {
	switch (s) {
	case AssetSeverity::Error:             return {1, 0.2f, 0.2f, 1};
	case AssetSeverity::Warning:           return {1, 0.8f, 0.1f, 1};
	case AssetSeverity::TransitiveWarning: return {0.9f, 0.65f, 0.3f, 1};
	case AssetSeverity::Info:              return {0.7f, 0.7f, 0.7f, 1};
	}
	return {1, 1, 1, 1};
}

static const char* severity_label(AssetSeverity s) {
	switch (s) {
	case AssetSeverity::Error:             return "ERR";
	case AssetSeverity::Warning:           return "WRN";
	case AssetSeverity::TransitiveWarning: return "DEP";
	case AssetSeverity::Info:              return "INF";
	}
	return "???";
}

static bool severity_passes_filter(AssetSeverity s, bool err, bool warn, bool trans, bool info) {
	switch (s) {
	case AssetSeverity::Error:             return err;
	case AssetSeverity::Warning:           return warn;
	case AssetSeverity::TransitiveWarning: return trans;
	case AssetSeverity::Info:              return info;
	}
	return false;
}

void DiagnosticsWindow::imgui_draw() {
	if (!is_open) return;

	ImGui::SetNextWindowSize({700, 500}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Diagnostics", &is_open)) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("DiagTabs")) {
		if (ImGui::BeginTabItem("Asset Diagnostics")) {
			draw_asset_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Map Errors")) {
			draw_map_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void DiagnosticsWindow::draw_asset_tab() {
	ImGui::InputTextWithHint("##search", "Search assets...", search_filter, sizeof(search_filter));
	ImGui::SameLine();

	ImGui::Checkbox("Err", &show_error); ImGui::SameLine();
	ImGui::Checkbox("Wrn", &show_warning); ImGui::SameLine();
	ImGui::Checkbox("Dep", &show_transitive); ImGui::SameLine();
	ImGui::Checkbox("Inf", &show_info);

	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		AssetDiagnostics::get().scan_all();
	}

	ImGui::Separator();

	struct Row {
		const std::string* path;
		const AssetDiagnostic* diag;
	};
	std::vector<Row> rows;

	auto& all = AssetDiagnostics::get().get_all();
	for (auto& [path, diags] : all) {
		for (auto& d : diags) {
			if (!severity_passes_filter(d.severity, show_error, show_warning, show_transitive, show_info))
				continue;
			if (search_filter[0] && !contains_ci(path.c_str(), search_filter)
				&& !contains_ci(d.message.c_str(), search_filter))
				continue;
			rows.push_back({&path, &d});
		}
	}

	std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
		if (a.diag->severity != b.diag->severity)
			return a.diag->severity > b.diag->severity;
		return *a.path < *b.path;
	});

	int counts[4] = {};
	for (auto& [path, diags] : all)
		for (auto& d : diags)
			counts[(int)d.severity]++;
	ImGui::Text("%d error, %d warning, %d transitive, %d info  |  %d shown",
		counts[(int)AssetSeverity::Error], counts[(int)AssetSeverity::Warning],
		counts[(int)AssetSeverity::TransitiveWarning], counts[(int)AssetSeverity::Info],
		(int)rows.size());

	ImGui::Separator();

	if (ImGui::BeginTable("AssetDiagTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Sev", ImGuiTableColumnFlags_WidthFixed, 40);
		ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin((int)rows.size());
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
				auto& r = rows[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextColored(severity_color(r.diag->severity), "%s",
					severity_label(r.diag->severity));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(r.path->c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(r.diag->message.c_str());
			}
		}

		ImGui::EndTable();
	}
}

void DiagnosticsWindow::draw_map_tab() {
	auto* level = eng ? eng->get_level() : nullptr;
	if (!level) {
		ImGui::TextDisabled("No level loaded.");
		return;
	}

	if (ImGui::Button("Refresh")) {
		level->validate();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Map: %s", level->get_source_asset_name().c_str());

	ImGui::Separator();

	// --- Unresolved Entities ---
	int n_unresolved = (int)level->preserved_unknown_objs.size();
	if (ImGui::CollapsingHeader(
		("Unresolved Entities (" + std::to_string(n_unresolved) + ")###unresolved").c_str(),
		ImGuiTreeNodeFlags_DefaultOpen)) {
		if (n_unresolved == 0) {
			ImGui::TextDisabled("None");
		} else {
			ImGui::TextDisabled("Entities whose type wasn't found at load. Preserved on save.");
			for (auto& blob : level->preserved_unknown_objs) {
				const char* type = "<no __typename>";
				std::string buf;
				auto it = blob.find("__typename");
				if (it != blob.end() && it->is_string()) {
					buf = it->get<std::string>();
					type = buf.c_str();
				}
				ImGui::BulletText("%s", type);
			}
		}
	}

	// --- Unknown Fields ---
	int n_fields = (int)level->unknown_field_warnings.size();
	if (ImGui::CollapsingHeader(
		("Unknown Fields (" + std::to_string(n_fields) + ")###fields").c_str(),
		ImGuiTreeNodeFlags_DefaultOpen)) {
		if (n_fields == 0) {
			ImGui::TextDisabled("None");
		} else {
			ImGui::TextDisabled("JSON keys with no matching reflected property.");
			for (auto& w : level->unknown_field_warnings)
				ImGui::BulletText("%s", w.c_str());
		}
	}

	// --- Duplicate Entity Names ---
	std::unordered_map<std::string, int> name_counts;
	for (auto obj : level->get_all_objects()) {
		auto* ent = obj->cast_to<Entity>();
		if (!ent) continue;
		auto& name = ent->get_editor_name();
		if (!name.empty())
			name_counts[name]++;
	}
	std::vector<std::pair<std::string, int>> dupes;
	for (auto& [name, count] : name_counts)
		if (count > 1)
			dupes.push_back({name, count});
	std::sort(dupes.begin(), dupes.end());

	if (ImGui::CollapsingHeader(
		("Duplicate Names (" + std::to_string(dupes.size()) + ")###dupes").c_str(),
		ImGuiTreeNodeFlags_DefaultOpen)) {
		if (dupes.empty()) {
			ImGui::TextDisabled("None");
		} else {
			ImGui::TextDisabled("Multiple entities share the same name.");
			for (auto& [name, count] : dupes)
				ImGui::BulletText("\"%s\" x%d", name.c_str(), count);
		}
	}
}

#endif
