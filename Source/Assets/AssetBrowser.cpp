#ifdef EDITOR_BUILD
#include "AssetBrowser.h"

#include "imgui.h"
#include <algorithm>
#include "Framework/MyImguiLib.h"
#include "Framework/Config.h"

#include "AssetCompile/Someutils.h"
#include "AssetRegistryLocal.h"

#include "AssetDatabase.h"
AssetBrowser::AssetBrowser()
{
	asset_name_filter[0] = 0;
	folder_closed = g_assets.find_global_sync<Texture>("eng/editor/folder_closed.png").get();
	folder_open = g_assets.find_global_sync<Texture>("eng/editor/folder_open.png").get();
	if (!folder_closed || !folder_open)
		Fatalf("no folder icons\n");

	commands = ConsoleCmdGroup::create("");
	commands->add("CLEAR_AB_FILTER", [this](const Cmd_Args&) { clear_filter(); });
	commands->add("FILTER_FOR", [this](const Cmd_Args& args) { 
		if (args.size() != 2) {
			sys_print(Warning, "FILTER_FOR <asset type>\n");
			return;
		}
		auto type = AssetRegistrySystem::get().find_type(args.at(1));
		if (!type)
		{
			sys_print(Warning, "no FILTER_FOR type name\n");
			return;
		}
		filter_all();
		unset_filter(1 << type->self_index);
		});

}
AssetBrowser* AssetBrowser::inst = nullptr;

static void draw_browser_tree_view_R(AssetBrowser* b, int indents, AssetFilesystemNode* node)
{
	const float folder_indent = 20.0;
	const int name_filter_len = strlen(b->asset_name_filter);
	for (auto node : node->sorted_list)
	{
		// leaf node
		if (node->children.empty()) {
			auto& asset = node->asset;
			if (!b->should_type_show(1 << asset.type->self_index)) {
				continue;
			}
			if (!b->filter_match_case && name_filter_len > 0) {
				std::string path = asset.filename;
				for (int i = 0; i < path.size(); i++) path[i] = tolower(path[i]);
				if (path.find(b->all_lower_cast_filter_name, 0) == std::string::npos)
					continue;
			}
			else if (name_filter_len > 0) {
				if (asset.filename.find(b->asset_name_filter) == std::string::npos)
					continue;
			}

			ImGui::PushID(node);

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			bool item_is_selected = b->selected_resource.filename == asset.filename;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
				b->selected_resource = asset;
				auto type = asset.type;
				if (ImGui::GetIO().MouseClickedCount[0] == 2) {
					if (type->tool_to_edit_me()) {
						std::string cmdstr = "start_ed ";
						cmdstr += '"';
						cmdstr += type->get_type_name();
						cmdstr += '"';
						cmdstr += " ";
						cmdstr += '"';
						cmdstr += asset.filename;
						cmdstr += '"';
						Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmdstr.c_str());
					}
				}
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				b->drag_drop = asset;
				auto ptr = &b->drag_drop;


				ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));

				ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s", asset.type->get_type_name().c_str());
				ImGui::Text("Asset: %s", asset.filename.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(indents * folder_indent, 0.1));
			ImGui::SameLine();
			ImGui::Text(node->name.c_str());
			ImGui::TableNextColumn();
			ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s", asset.type->get_type_name().c_str());

			ImGui::PopID();
		}
		else {
			ImGui::PushID(node);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			// folder node
			ImGui::Dummy(ImVec2(indents * folder_indent, 0.1));
			ImGui::SameLine();
			//int flags = ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
			//if (node->folder_is_open)
			//	flags |= ImGuiTreeNodeFlags_DefaultOpen;
			//node->folder_is_open = ImGui::TreeNodeEx("##folder", flags);
			bool booldummy = false;
			int flagssel = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##folder", &booldummy, flagssel))
				node->folder_is_open = !node->folder_is_open;
			//if(node->folder_is_open)
			//	ImGui::TreePop();
			ImGui::SameLine();
			auto t = node->folder_is_open ? b->folder_open : b->folder_closed;
			ImGui::Image(ImTextureID(uint64_t(t->gl_id)), ImVec2(t->width, t->height));
			ImGui::SameLine();
			ImGui::Text(node->name.c_str());

			ImGui::TableNextColumn();

			ImGui::PopID();

			if(node->folder_is_open)
				draw_browser_tree_view_R(b, indents + 1, node);
		}

	}
}

static void draw_browser_tree_view(AssetBrowser* b)
{
	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	if (ImGui::BeginTable("Browser", 2, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);

		ImGui::TableHeadersRow();

		
		auto root = AssetRegistrySystem::get().get_root_files();
		if(root)
			draw_browser_tree_view_R(b, 0, root);

		ImGui::EndTable();
	}
}


AssetFilesystemNode* AssetBrowser::find_node_for_asset(const std::string& path) const
{
	// brute force lols
	auto root = AssetRegistrySystem::get().get_root_files();
	auto recurse = [](auto&& self, const std::string& path, AssetFilesystemNode* n) -> AssetFilesystemNode* {
		if (!n->is_folder()) {
			if (n->asset.filename == path)
				return n;
		}
		for (auto& c : n->children) {
			auto r = self(self,path, &c.second);
			if (r)
				return r;
		}
		return nullptr;
	};
	return recurse(recurse,path, root);
}
void AssetBrowser::set_selected(const std::string& path)
{
	auto find = find_node_for_asset(path);
	if (!find) {
		sys_print(Error, "couldnt find to select\n");
		return;
	}
	auto f = find;
	while (f) {
		f->folder_is_open = true;
		f = f->parent;
	}
	selected_resource.filename = path;

	int len = std::min(255, (int)path.size());
	memcpy(asset_name_filter, path.c_str(), len);
	asset_name_filter[len] = 0;
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
	filter_match_case = match_case;

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
	ImGui::SameLine();
	if (ImGui::SmallButton("Expand All")) {
		AssetRegistrySystem::get().get_root_files()->set_folder_open_R(true);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Close All")) {
		AssetRegistrySystem::get().get_root_files()->set_folder_open_R(false);
	}


	if (!match_case) {
		all_lower_cast_filter_name = asset_name_filter;
		for (int i = 0; i < name_filter_len; i++)
			all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
	}

	draw_browser_tree_view(this);

	ImGui::End();

}


#endif