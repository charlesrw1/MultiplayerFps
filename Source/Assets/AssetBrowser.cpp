#include "AssetBrowser.h"

#include "imgui.h"
#include <algorithm>
#include "Framework/MyImguiLib.h"
#include "Framework/Config.h"
AssetBrowser global_asset_browser;

static std::string to_lower(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (auto c : s)
		out.push_back(tolower(c));
	return out;
}

void AssetBrowser::init()
{
	AssetRegistrySystem::get().reindex_all_assets();
	asset_name_filter[0] = 0;
}

void AssetBrowser::imgui_draw()
{
	double_clicked_selected = false;
	if(force_focus)
		ImGui::SetNextWindowFocus();
	force_focus = false;
	if (!ImGui::Begin("Asset Browser")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

	//if (set_keyboard_focus) {
	//	ImGui::SetKeyboardFocusHere();
	//	set_keyboard_focus = false;
	//}
	static bool match_case = false;
	ImGui::SetNextItemWidth(200.0);
	ImGui::InputTextWithHint("FILTER", "filter asset path", asset_name_filter, 256);
	ImGui::SameLine();
	ImGui::Checkbox("MATCH CASE", &match_case);
	const int name_filter_len = strlen(asset_name_filter);

	if (show_filter_type_options && ImGui::SmallButton("Type filters..."))
		ImGui::OpenPopup("type_popup_assets");
	if (ImGui::BeginPopup("type_popup_assets"))
	{
		bool is_hiding_all = filter_type_mask != 0;
		if (ImGui::Checkbox("Show/Hide all", &is_hiding_all)) {
			if (filter_type_mask != 0)
				filter_type_mask = 0;
			else
				filter_type_mask = -1;
		}

		auto& types = AssetRegistrySystem::get().get_types();
		for (int i = 0; i < types.size(); i++) {
			ImGui::CheckboxFlags(types[i]->get_type_name().c_str(), &filter_type_mask, 1 << types[i]->self_index);
		}
		ImGui::EndPopup();
	}


	std::string all_lower_cast_filter_name;
	if (!match_case) {
		all_lower_cast_filter_name = asset_name_filter;
		for (int i = 0; i < name_filter_len; i++)
			all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
	}

	if (ImGui::BeginTable("Browser", 2, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);

		auto& resources = AssetRegistrySystem::get().get_all_assets();
		if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
			if (sorts_specs->SpecsDirty)
			{

				std::sort(resources.begin(), resources.end(),
					[&](const AssetOnDisk& a, const AssetOnDisk& b) -> bool {
						if (sorts_specs->Specs[0].ColumnIndex == 0 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending)
							return to_lower(a.filename) > to_lower(b.filename);
						else if (sorts_specs->Specs[0].ColumnIndex == 0 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Descending)
							return to_lower(a.filename) < to_lower(b.filename);
						else if (sorts_specs->Specs[0].ColumnIndex == 1 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending)
							return to_lower(a.type->get_type_name()) > to_lower(b.type->get_type_name());
						else if (sorts_specs->Specs[0].ColumnIndex == 1 && sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Descending)
							return to_lower(a.type->get_type_name()) < to_lower(b.type->get_type_name());
						return true;
					}
				);
				sorts_specs->SpecsDirty = false;
			}


		ImGui::TableHeadersRow();

		for (int row_n = 0; row_n < resources.size(); row_n++)
		{
			auto res = resources[row_n];
			if (!should_type_show(1 << res.type->self_index))
				continue;
			if (!match_case && name_filter_len > 0) {
				std::string path = res.filename;
				for (int i = 0; i < path.size(); i++) path[i] = tolower(path[i]);
				if (path.find(all_lower_cast_filter_name, 0) == std::string::npos)
					continue;
			}
			else if (name_filter_len > 0) {
				if (res.filename.find(asset_name_filter) == std::string::npos)
					continue;
			}
			ImGui::PushID(res.filename.c_str());
			const bool item_is_selected = res.filename == selected_resource.filename;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
				selected_resource = res;
				if (ImGui::GetIO().MouseClickedCount[0] == 2) {
					double_clicked_selected = true;
					if (selected_resource.type->tool_to_edit_me()) {
						std::string cmdstr = "start_ed ";
						cmdstr += selected_resource.type->get_type_name();
						cmdstr += " ";
						cmdstr += selected_resource.filename;
						Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmdstr.c_str());
					}
				}
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				drag_drop = res;
				auto ptr = &drag_drop;


				ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));

				ImGui::TextColored(color32_to_imvec4(res.type->get_browser_color()), "%s", res.type->get_type_name().c_str());
				ImGui::Text("Asset: %s", res.filename.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Text(res.filename.c_str());
			ImGui::TableNextColumn();
			ImGui::TextColored(color32_to_imvec4(res.type->get_browser_color()), "%s", res.type->get_type_name().c_str());

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();

}
