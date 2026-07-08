#ifdef EDITOR_BUILD
#include "AssetBrowser.h"
#include "Assets/AssetInspectorPane.h"
#include "AssetTools/AssetDiagnostics.h"
#include "AssetTools/AssetOps.h"

#include "imgui.h"
#include <algorithm>
#include <optional>
#include "Framework/MyImguiLib.h"
#include "Framework/Config.h"

#include "AssetCompile/Someutils.h"
#include "AssetRegistryLocal.h"

#include "AssetDatabase.h"

// Case-insensitive substring search without allocations
static inline bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
	if (needle.empty())
		return true;
	if (haystack.size() < needle.size())
		return false;

	for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
		bool match = true;
		for (size_t j = 0; j < needle.size(); ++j) {
			if (tolower(haystack[i + j]) != tolower(needle[j])) {
				match = false;
				break;
			}
		}
		if (match)
			return true;
	}
	return false;
}

AssetBrowser::AssetBrowser() {
	asset_name_filter[0] = 0;
	folder_closed = g_assets.find<Texture>("eng/editor/folder_closed.png").get();
	folder_open = g_assets.find<Texture>("eng/editor/folder_open.png").get();
	if (!folder_closed || !folder_open)
		Fatalf("no folder icons\n");
	import_model_icon = g_assets.find<Texture>("eng/icons/publish.png").get();

	inspector_pane = std::make_unique<AssetInspectorPane>();

	commands = ConsoleCmdGroup::create("");
	commands->add("CLEAR_AB_FILTER", [this](const Cmd_Args&) { clear_filter(); });
	commands->add("ASSET_DIAG_LOG", [](const Cmd_Args&) {
		auto& diag = AssetDiagnostics::get();
		auto& all = diag.get_all();
		if (all.empty()) {
			sys_print(Info, "No asset diagnostics.\n");
			return;
		}
		int errors = 0, warnings = 0, transitive = 0, infos = 0;
		for (auto& [path, diags] : all) {
			for (auto& d : diags) {
				const char* label;
				switch (d.severity) {
				case AssetSeverity::Error:   label = "ERR"; errors++; break;
				case AssetSeverity::Warning: label = "WRN"; warnings++; break;
				case AssetSeverity::Info:    label = "INF"; infos++; break;
				default:                     label = "DEP"; transitive++; break;
				}
				if (d.severity > AssetSeverity::Info)
					sys_print(Warning, "[%s] %s: %s\n", label, path.c_str(), d.message.c_str());
			}
		}
		sys_print(Info, "Asset diagnostics: %d errors, %d warnings, %d transitive, %d info\n", errors, warnings, transitive, infos);
	});
	commands->add("FILTER_FOR", [this](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "FILTER_FOR <asset type>\n");
			return;
		}
		auto type = AssetRegistrySystem::get().find_type(args.at(1));
		if (!type) {
			sys_print(Warning, "no FILTER_FOR type name\n");
			return;
		}
		filter_all();
		unset_filter(1 << type->self_index);
	});
}
AssetBrowser* AssetBrowser::inst = nullptr;
ConfigVar ignore_folders("ignore_folders", "0", CVAR_BOOL, "");
#include "GameEnginePublic.h"
static void draw_browser_tree_view_R(AssetBrowser* b, int indents, AssetFilesystemNode* node, string parent_path) {
	const float folder_indent = 20.0;
	const int name_filter_len = strlen(b->asset_name_filter);
	for (auto node : node->sorted_list) {
		// leaf node
		if (node->children.empty()) {
			auto& asset = node->asset;
			if (!b->should_type_show(1 << asset.type->self_index)) {
				continue;
			}
			if (name_filter_len > 0) {
				if (b->filter_match_case) {
					if (asset.filename.find(b->asset_name_filter) == std::string::npos)
						continue;
				} else {
					if (!contains_case_insensitive(asset.filename, b->all_lower_cast_filter_name))
						continue;
				}
			}

			ImGui::PushID(node);

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick |
													ImGuiSelectableFlags_SpanAllColumns |
													ImGuiSelectableFlags_AllowItemOverlap;
			bool item_is_selected = b->selected_resource.filename == asset.filename;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
				b->selected_resource = asset;
				auto type = asset.type;

				if (ImGui::GetIO().MouseClicked[2]) {
					ImGui::OpenPopup("asset-click-menu");
				}
				if (ImGui::GetIO().MouseClickedCount[0] == 2) {
					// auto assetType = type->get_asset_class_type();
					if (type->get_type_name() == "Map" || type->get_type_name() == "Prefab") {
						Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("open-editor " + asset.filename).c_str());
					} else {
						sys_print(Warning, "AssetBrowser: Asset is not a standard IAsset, can't edit.\n");
					}
				}
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				b->drag_drop = asset;
				auto ptr = &b->drag_drop;

				ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));

				ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s",
								   asset.type->get_type_name().c_str());
				ImGui::Text("Asset: %s", asset.filename.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(indents * folder_indent, 0.1));
			ImGui::SameLine();
			if (ignore_folders.get_bool()) {
				string name = parent_path + "/" + node->name;
				ImGui::Text(name.c_str());

			} else {
				ImGui::Text(node->name.c_str());
			}

			ImGui::TableNextColumn();
			ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s",
							   asset.type->get_type_name().c_str());

			ImGui::PopID();
		} else {
			if (!ignore_folders.get_bool()) {

				ImGui::PushID(node);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				// folder node
				ImGui::Dummy(ImVec2(indents * folder_indent, 0.1));
				ImGui::SameLine();
				bool booldummy = false;
				int flagssel = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
				if (ImGui::Selectable("##folder", &booldummy, flagssel))
					node->folder_is_open = !node->folder_is_open;
				ImGui::SameLine();
				auto t = node->folder_is_open ? b->folder_open : b->folder_closed;
				my_imgui_image(t, -1);
				ImGui::SameLine();
				ImGui::Text(node->name.c_str());

				ImGui::TableNextColumn();

				ImGui::PopID();
			}
			int add_indent = 1;
			if (ignore_folders.get_bool())
				add_indent = 0;

			if (node->folder_is_open || ignore_folders.get_bool())
				draw_browser_tree_view_R(b, indents + add_indent, node, parent_path + "/" + node->name);
		}
	}
}
extern void OpenInNotepad(const string& name);
extern void SetClipboardText(const string& name);
extern void ShowInExplorer(const string& name);
extern std::optional<std::string> OpenGlbFileDialog();

static void draw_diag_tooltip(const std::string& gamepath) {
	auto* diags = AssetDiagnostics::get().get_diags(gamepath);
	if (!diags || !ImGui::IsItemHovered()) return;
	ImGui::BeginTooltip();
	for (auto& d : *diags) {
		ImVec4 col;
		const char* label;
		switch (d.severity) {
		case AssetSeverity::Error:             col = ImVec4(1, 0.2f, 0.2f, 1); label = "ERR"; break;
		case AssetSeverity::Warning:           col = ImVec4(1, 0.8f, 0.1f, 1); label = "WRN"; break;
		case AssetSeverity::Info:              col = ImVec4(0.8f, 0.8f, 0.8f, 1); label = "INF"; break;
		default:                               col = ImVec4(0.9f, 0.65f, 0.3f, 1); label = "DEP"; break;
		}
		ImGui::TextColored(col, "[%s]", label);
		ImGui::SameLine();
		ImGui::TextUnformatted(d.message.c_str());
	}
	ImGui::EndTooltip();
}

static void draw_create_new_menu_items(AssetBrowser* b, const std::string& folder);

static void draw_browser_tree_view_R2(AssetBrowser* b, int indents, AssetFilesystemNode* node, string parent_path) {
	const float folder_indent = 20.0;
	const int name_filter_len = strlen(b->asset_name_filter);

	// leaf node
	if (node->children.empty()) {
		auto& asset = node->asset;

		ImGui::PushID(node);

		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick |
												ImGuiSelectableFlags_SpanAllColumns |
												ImGuiSelectableFlags_AllowItemOverlap;
		bool item_is_selected = b->selected_resource.filename == asset.filename;
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			b->selected_resource = asset;
			auto type = asset.type;
			if (ImGui::GetIO().MouseClickedCount[0] == 2) {
				// auto assetType = type->get_asset_class_type();
				if (type->get_type_name() == "Map" || type->get_type_name() == "Prefab") {
					Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("open-editor " + asset.filename).c_str());
				} else {
					sys_print(Warning, "AssetBrowser: Asset is not a standard IAsset, can't edit.\n");
				}
			}
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
			b->selected_resource = asset;
			ImGui::OpenPopup("asset-click-menu");
		}

		if (ImGui::BeginPopup("asset-click-menu")) {
			ImGui::Text("%s", b->selected_resource.filename.c_str());
			ImGui::Separator();
			if (ImGui::MenuItem("Copy to clipboard")) {
				SetClipboardText(b->selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::MenuItem("Open in notepad")) {
				OpenInNotepad(b->selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::MenuItem("Show in Explorer")) {
				ShowInExplorer(b->selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (b->selected_resource.type) {
				ImGui::Separator();
				b->selected_resource.type->draw_browser_context_menu(b->selected_resource.filename);
			}
			ImGui::Separator();
			draw_create_new_menu_items(b, b->selected_folder);
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			b->drag_drop = asset;
			auto ptr = &b->drag_drop;

			ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));

			ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s",
							   asset.type->get_type_name().c_str());
			ImGui::Text("Asset: %s", asset.filename.c_str());

			ImGui::EndDragDropSource();
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(indents * folder_indent, 0.1));
		ImGui::SameLine();

		// Error indicator prefix
		auto sev = AssetDiagnostics::get().get_severity(asset.filename);
		if (sev) {
			ImVec4 col;
			switch (*sev) {
			case AssetSeverity::Error:            col = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
			case AssetSeverity::Warning:          col = ImVec4(1.0f, 0.8f, 0.1f, 1.0f); break;
			case AssetSeverity::TransitiveWarning: col = ImVec4(0.9f, 0.65f, 0.3f, 1.0f); break;
			case AssetSeverity::Info:             col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
			}
			ImGui::TextColored(col, *sev == AssetSeverity::Info ? "[?]" : "[!]");
			draw_diag_tooltip(asset.filename);
			ImGui::SameLine();
		}

		if (ignore_folders.get_bool()) {
			string name = parent_path + "/" + node->name;
			ImGui::Text(name.c_str());

		} else {
			ImGui::Text(node->name.c_str());
		}

		ImGui::TableNextColumn();
		ImGui::TextColored(color32_to_imvec4(asset.type->get_browser_color()), "%s",
						   asset.type->get_type_name().c_str());

		ImGui::PopID();
	}
}

static void draw_browser_tree_view(AssetBrowser* b) {
	auto& linear = AssetRegistrySystem::get().get_linear_list();
	const int name_filter_len = strlen(b->asset_name_filter);
	std::vector<AssetFilesystemNode*> linear2;
	for (auto node : linear) {
		auto& asset = node->asset;
		if (!b->should_type_show(1 << asset.type->self_index)) {
			continue;
		}
		if (!b->selected_folder.empty()) {
			if (asset.filename.find(b->selected_folder + "/") != 0)
				continue;
		}
		if (name_filter_len > 0) {
			if (b->filter_match_case) {
				if (asset.filename.find(b->asset_name_filter) == std::string::npos)
					continue;
			} else {
				if (!contains_case_insensitive(asset.filename, b->all_lower_cast_filter_name))
					continue;
			}
		}
		linear2.push_back(node);
	}

	ImGuiListClipper clipper;
	clipper.Begin(linear2.size());

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
							  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	if (ImGui::BeginTable("Browser", 2, ent_list_flags)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);

		ImGui::TableHeadersRow();

		while (clipper.Step()) {

			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				draw_browser_tree_view_R2(b, 0, linear2.at(i), "");
			}
		}

		ImGui::EndTable();
	}

	clipper.End();
}

static void draw_create_new_menu_items(AssetBrowser* b, const std::string& folder) {
	if (ImGui::BeginMenu("Create New...")) {
		if (ImGui::MenuItem("Map")) {
			b->create_asset_type = AssetBrowser::CreateAssetType::Map;
			b->create_folder_override = folder;
			memset(b->create_asset_name, 0, sizeof(b->create_asset_name));
		}
		if (ImGui::MenuItem("Particle")) {
			b->create_asset_type = AssetBrowser::CreateAssetType::Particle;
			b->create_folder_override = folder;
			memset(b->create_asset_name, 0, sizeof(b->create_asset_name));
		}
		if (ImGui::MenuItem("Master Material")) {
			b->create_asset_type = AssetBrowser::CreateAssetType::MasterMaterial;
			b->create_folder_override = folder;
			b->create_mm_domain = 0;
			memset(b->create_asset_name, 0, sizeof(b->create_asset_name));
		}
		if (ImGui::MenuItem("Material Instance")) {
			b->create_asset_type = AssetBrowser::CreateAssetType::MaterialInstance;
			b->create_folder_override = folder;
			b->create_mi_master_path = "eng/fallback.mm";
			memset(b->create_asset_name, 0, sizeof(b->create_asset_name));
		}
		if (ImGui::MenuItem("Prefab")) {
			b->create_asset_type = AssetBrowser::CreateAssetType::Prefab;
			b->create_folder_override = folder;
			memset(b->create_asset_name, 0, sizeof(b->create_asset_name));
		}
		ImGui::EndMenu();
	}
}

// Recursive folder-only tree for the left panel.
// parent_path is the gamepath prefix (e.g. "" for root, "textures" for the textures folder).
static void draw_folder_tree_R(AssetBrowser* b, int indent, AssetFilesystemNode* node, const std::string& parent_path) {
	const float folder_indent = 16.0f;
	for (auto* child : node->sorted_list) {
		if (child->children.empty())
			continue; // leaf, skip

		std::string folder_path = parent_path.empty() ? child->name : (parent_path + "/" + child->name);

		ImGui::PushID(child);

		bool is_selected = (b->selected_folder == folder_path);

		ImGui::Dummy(ImVec2(indent * folder_indent, 1.0f));
		ImGui::SameLine();
		const float ICON_SIZE = 14.0f;

		auto* t = child->folder_is_open ? b->folder_open : b->folder_closed;
		if (t) {
			ImVec2 icon_pos = ImGui::GetCursorScreenPos();
			ImGui::PushID("##icon");
			if (ImGui::InvisibleButton("##toggle", ImVec2(ICON_SIZE, ICON_SIZE)))
				child->folder_is_open = !child->folder_is_open;
			ImGui::PopID();
			ImGui::GetWindowDrawList()->AddImage(
				ImTextureID(uint64_t(t->get_internal_render_handle())),
				icon_pos, ImVec2(icon_pos.x + ICON_SIZE, icon_pos.y + ICON_SIZE));
		}
		ImGui::SameLine();

		if (ImGui::Selectable(child->name.c_str(), is_selected, ImGuiSelectableFlags_AllowItemOverlap))
			b->selected_folder = folder_path;

		if (ImGui::BeginPopupContextItem()) {
			ImGui::TextDisabled("%s", folder_path.c_str());
			ImGui::Separator();
			draw_create_new_menu_items(b, folder_path);
			ImGui::EndPopup();
		}

		// Drag-drop target: accept file drops to move asset into this folder
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop")) {
				ASSERT(payload->DataSize == sizeof(AssetOnDisk*));
				const AssetOnDisk* src = *static_cast<const AssetOnDisk* const*>(payload->Data);
				auto result = AssetOps::mv(src->filename, folder_path);
				if (!result.success)
					sys_print(Error, "AssetOps::mv failed: %s\n", result.error.c_str());
			}
			ImGui::EndDragDropTarget();
		}

		if (child->folder_is_open)
			draw_folder_tree_R(b, indent + 1, child, folder_path);

		ImGui::PopID();
	}
}

static void draw_folder_tree(AssetBrowser* b) {
	auto* root = AssetRegistrySystem::get().get_root_files();
	if (!root)
		return;

	// "All" root entry
	bool all_selected = b->selected_folder.empty();
	if (ImGui::Selectable("(All)", all_selected))
		b->selected_folder.clear();

	draw_folder_tree_R(b, 0, root, "");
}

// too much of a brainlet do the dumb thing
void fill_big_vector(std::vector<AssetFilesystemNode*>& nodes, AssetFilesystemNode* root) {
	nodes = AssetRegistrySystem::get().get_linear_list();
}
#include "Render/Model.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"

bool ImageButtonWithOverlayText(ImTextureID texture, ImVec2 size, const char* label) {
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 label_pos = pos;

	// Invisible button to handle interaction
	ImGui::InvisibleButton(label, size);
	bool hovered = ImGui::IsItemHovered();
	bool pressed = ImGui::IsItemDeactivated() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);

	// Draw the image
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddImage(texture, pos, ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 1), ImVec2(1, 0));

	// Optional highlight on hover
	if (hovered) {
		draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 50));
	}

	// Draw wrapped text over image
	float wrap_width = size.x - 10.0f;					  // small padding
	ImVec2 text_pos = ImVec2(pos.x + 5.0f, pos.y + 5.0f); // small margin
	ImGui::PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
	ImGui::PushTextWrapPos(text_pos.x + wrap_width);
	auto shadow_pos = text_pos + ImVec2(1, 1);
	draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), shadow_pos, IM_COL32(0, 0, 0, 255), label, nullptr,
					   wrap_width);
	draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos, IM_COL32(255, 255, 255, 255), label, nullptr,
					   wrap_width);

	ImGui::PopTextWrapPos();
	ImGui::PopClipRect();

	return pressed;
}

void AssetBrowser::draw_browser_grid() {

	const int SIZE_PER = (big_thumbnail) ? 144 : 80;
	auto win_size = ImGui::GetWindowSize();
	int boxes = win_size.x / SIZE_PER;
	boxes = std::max(boxes, 1);

	uint32_t ent_list_flags =
		ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

	std::vector<AssetFilesystemNode*> items2;
	{
		std::vector<AssetFilesystemNode*> items;
		fill_big_vector(items, AssetRegistrySystem::get().get_root_files());

		const int name_filter_len = strlen(asset_name_filter);
		for (auto& c : items) {
			auto& asset = c->asset;
			if (!ThumbnailManager::supports_thumbnail(asset))
				continue;
			if (!selected_folder.empty()) {
				if (asset.filename.find(selected_folder + "/") != 0)
					continue;
			}
			if (!filter_match_case && name_filter_len > 0) {
				std::string path = asset.filename;
				for (int i = 0; i < (int)path.size(); i++)
					path[i] = tolower(path[i]);
				if (path.find(all_lower_cast_filter_name, 0) == std::string::npos)
					continue;
			} else if (name_filter_len > 0) {
				if (asset.filename.find(asset_name_filter) == std::string::npos)
					continue;
			}
			items2.push_back(c);
		}
	}

	ImGuiListClipper clipper;
	clipper.Begin((int)glm::ceil(items2.size() / float(boxes)));

	auto draw_item = [&](const int item_idx) {
		auto& c = items2.at(item_idx);
		ImGui::TableNextColumn();
		ImGui::PushID(c);

		Texture* t = thumbnails.get_thumbnail(c->asset); // marks as visible; may be null (loading)
		string only_filename = c->asset.filename;
		StringUtils::get_filename(only_filename);

		const int THUMB_SIZE = (big_thumbnail) ? 128 : 64;
		ImVec2 thumb_screen_pos = ImGui::GetCursorScreenPos();
		ImVec2 thumb_size = ImVec2((float)THUMB_SIZE, (float)THUMB_SIZE);

		const bool is_selected = (selected_resource.filename == c->asset.filename);
		bool item_pressed = false;

		if (t) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			item_pressed = ImageButtonWithOverlayText(ImTextureID(uint64_t(t->get_internal_render_handle())),
													  thumb_size, only_filename.c_str());
			ImGui::PopStyleColor();
		} else {
			// Placeholder while streaming in
			ImGui::InvisibleButton("##thumb", thumb_size);
			item_pressed = ImGui::IsItemDeactivated() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 bmax = ImVec2(thumb_screen_pos.x + THUMB_SIZE, thumb_screen_pos.y + THUMB_SIZE);
			dl->AddRectFilled(thumb_screen_pos, bmax, IM_COL32(35, 35, 35, 255));
			dl->AddRect(thumb_screen_pos, bmax, IM_COL32(60, 60, 60, 255));
			ImGui::PushClipRect(thumb_screen_pos, bmax, true);
			dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
						ImVec2(thumb_screen_pos.x + 4.f, thumb_screen_pos.y + 4.f),
						IM_COL32(110, 110, 110, 255), only_filename.c_str(), nullptr, THUMB_SIZE - 8.f);
			ImGui::PopClipRect();
		}

		// Selection highlight border
		if (is_selected) {
			ImVec2 bmax = ImVec2(thumb_screen_pos.x + THUMB_SIZE, thumb_screen_pos.y + THUMB_SIZE);
			ImGui::GetWindowDrawList()->AddRect(thumb_screen_pos, bmax, IM_COL32(100, 180, 255, 230), 0.f, 0, 2.f);
		}

		// Left-click: select; double-click: open
		if (item_pressed) {
			selected_resource = c->asset;
		}
		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClickedCount[0] == 2) {
			selected_resource = c->asset;
			double_clicked_selected = true;
			const auto& tname = c->asset.type->get_type_name();
			if (tname == "Map" || tname == "Prefab")
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("open-editor " + c->asset.filename).c_str());
		}

		// Hover tooltip: full path + type after short delay
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
			ImGui::BeginTooltip();
			ImGui::TextColored(color32_to_imvec4(c->asset.type->get_browser_color()), "%s",
							   c->asset.type->get_type_name().c_str());
			ImGui::TextUnformatted(c->asset.filename.c_str());
			ImGui::EndTooltip();
		}

		// Right-click: select and open context menu
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			selected_resource = c->asset;
			ImGui::OpenPopup("asset-click-menu");
		}
		if (ImGui::BeginPopup("asset-click-menu")) {
			ImGui::Text("%s", selected_resource.filename.c_str());
			ImGui::Separator();
			if (ImGui::MenuItem("Copy to clipboard")) {
				SetClipboardText(selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::MenuItem("Open in notepad")) {
				OpenInNotepad(selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::MenuItem("Show in Explorer")) {
				ShowInExplorer(selected_resource.filename);
				ImGui::CloseCurrentPopup();
			}
			if (selected_resource.type) {
				ImGui::Separator();
				selected_resource.type->draw_browser_context_menu(selected_resource.filename);
			}
			ImGui::Separator();
			draw_create_new_menu_items(this, selected_folder);
			ImGui::EndPopup();
		}

		// Error/warning overlay badge in top-right corner of thumbnail
		auto sev = AssetDiagnostics::get().get_severity(c->asset.filename);
		if (sev) {
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImU32 badge_col;
			switch (*sev) {
			case AssetSeverity::Error:             badge_col = IM_COL32(220, 40, 40, 230);  break;
			case AssetSeverity::Warning:           badge_col = IM_COL32(220, 180, 0, 230);  break;
			case AssetSeverity::TransitiveWarning: badge_col = IM_COL32(200, 140, 40, 200); break;
			case AssetSeverity::Info:              badge_col = IM_COL32(180, 180, 180, 160); break;
			default:                               badge_col = IM_COL32(200, 140, 40, 200); break;
			}
			ImVec2 badge_min = ImVec2(thumb_screen_pos.x + THUMB_SIZE - 18, thumb_screen_pos.y + 2);
			ImVec2 badge_max = ImVec2(thumb_screen_pos.x + THUMB_SIZE - 2,  thumb_screen_pos.y + 16);
			dl->AddRectFilled(badge_min, badge_max, badge_col, 3.0f);
			dl->AddText(ImVec2(badge_min.x + 2, badge_min.y + 1), IM_COL32(255, 255, 255, 255), "!");
			draw_diag_tooltip(c->asset.filename);
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			drag_drop = c->asset;
			auto ptr = &drag_drop;
			ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));
			ImGui::TextColored(color32_to_imvec4(c->asset.type->get_browser_color()), "%s",
							   c->asset.type->get_type_name().c_str());
			ImGui::Text("Asset: %s", c->asset.filename.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
	};

	auto draw_in_row = [&](const int row) {
		ImGui::TableNextRow();
		const int start = row * boxes;
		for (int i = 0; i < boxes; i++) {
			const int index = start + i;
			if (index >= (int)items2.size())
				break;
			draw_item(index);
		}
	};

	if (ImGui::BeginTable("Browser", boxes, ent_list_flags)) {
		for (int i = 0; i < boxes; i++)
			ImGui::TableSetupColumn("##blah", ImGuiTableColumnFlags_WidthStretch);

		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				draw_in_row(i);
		}
		ImGui::EndTable();
	}
	clipper.End();
}

AssetFilesystemNode* AssetBrowser::find_node_for_asset(const std::string& path) const {
	// brute force lols
	auto root = AssetRegistrySystem::get().get_root_files();
	auto recurse = [](auto&& self, const std::string& path, AssetFilesystemNode* n) -> AssetFilesystemNode* {
		if (!n->is_folder()) {
			if (n->asset.filename == path)
				return n;
		}
		for (auto& c : n->children) {
			auto r = self(self, path, &c.second);
			if (r)
				return r;
		}
		return nullptr;
	};
	return recurse(recurse, path, root);
}
void AssetBrowser::set_selected(const std::string& path) {
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

void AssetBrowser::imgui_draw() {
	double_clicked_selected = false;
	if (force_focus)
		ImGui::SetNextWindowFocus();
	force_focus = false;
	if (!ImGui::Begin("Asset Browser")) {
		ImGui::End();
		return;
	}

	if (import_model_icon) {
		const float ICON_SIZE = 16.0f;
		ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_Button];
		if (ImGui::ImageButton("##import_model",
				ImTextureID(uint64_t(import_model_icon->get_internal_render_handle())),
				ImVec2(ICON_SIZE, ICON_SIZE), ImVec2(0, 0), ImVec2(1, 1), bg, ImVec4(0.3f, 0.75f, 0.35f, 1.0f)))
			import_model_dialog();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Import Model (.glb)");
		ImGui::SameLine();
	}

	// Filter bar
	static bool match_case = false;
	ImGui::SetNextItemWidth(200.0);
	ImGui::InputTextWithHint("FILTER", "filter asset path", asset_name_filter, 256);
	ImGui::SameLine();
	ImGui::Checkbox("MATCH CASE", &match_case);
	ImGui::SameLine();
	ImGui::Checkbox("Grid", &using_grid);
	if (using_grid) {
		ImGui::SameLine();
		ImGui::Checkbox("Big", &big_thumbnail);
	}
	const int name_filter_len = strlen(asset_name_filter);
	filter_match_case = match_case;

	if (show_filter_type_options && ImGui::SmallButton("Type filters..."))
		ImGui::OpenPopup("type_popup_assets");
	if (ImGui::BeginPopup("type_popup_assets")) {
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

	// Split layout: folder tree | asset view
	const float splitter_w = 6.0f;
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float main_h = std::max(avail.y, 40.0f);

	// Left panel: folder tree
	ImGui::BeginChild("##folder_tree", ImVec2(left_panel_width, main_h), true);
	draw_folder_tree(this);
	if (ImGui::BeginPopupContextWindow("create_asset_menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		ImGui::TextDisabled("Create in: %s", selected_folder.empty() ? "(root)" : selected_folder.c_str());
		ImGui::Separator();
		draw_create_new_menu_items(this, selected_folder);
		ImGui::EndPopup();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Vertical splitter handle
	ImGui::InvisibleButton("##vsplitter", ImVec2(splitter_w, main_h));
	if (ImGui::IsItemActive()) {
		left_panel_width += ImGui::GetIO().MouseDelta.x;
		left_panel_width = std::clamp(left_panel_width, 60.0f, avail.x - 120.0f);
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::SameLine();

	// Right panel: asset grid or list
	ImGui::BeginChild("##asset_view", ImVec2(0.0f, main_h), false);
	if (using_grid) {
		draw_browser_grid();
	} else {
		draw_browser_tree_view(this);
	}
	// Tick every frame so property-grid thumbnails load even without the grid view open.
	thumbnails.tick();
	ImGui::EndChild();

	draw_create_asset_popup();

	ImGui::End();
}

#include "AssetTools/AssetTemplates.h"

void AssetBrowser::draw_create_asset_popup() {
	if (create_asset_type == CreateAssetType::None)
		return;

	const char* titles[] = {"", "Create Map", "Create Particle", "Create Master Material", "Create Material Instance", "Create Prefab"};
	const char* title = titles[(int)create_asset_type];

	ImGui::OpenPopup(title);
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextDisabled("Folder: %s", create_folder_override.empty() ? "(root)" : create_folder_override.c_str());
		ImGui::Text("Name (no extension):");
		bool enter_pressed = ImGui::InputText("##name", create_asset_name, sizeof(create_asset_name),
			ImGuiInputTextFlags_EnterReturnsTrue);

		if (create_asset_type == CreateAssetType::MasterMaterial) {
			const char* domain_names[] = {"Default", "PostProcess", "Terrain", "Decal", "UI", "Particle"};
			ImGui::Combo("Domain", &create_mm_domain, domain_names, 6);
		}

		if (create_asset_type == CreateAssetType::MaterialInstance) {
			ImGui::Text("Master: %s", create_mi_master_path.c_str());
		}

		ImGui::Separator();

		bool do_create = enter_pressed || ImGui::Button("Create", ImVec2(120, 0));
		ImGui::SameLine();
		bool do_cancel = ImGui::Button("Cancel", ImVec2(120, 0));

		if (do_create && strlen(create_asset_name) > 0) {
			std::optional<std::string> result;
			switch (create_asset_type) {
			case CreateAssetType::Map:
				result = AssetTemplates::create_empty_map(create_folder_override, create_asset_name);
				break;
			case CreateAssetType::Particle:
				result = AssetTemplates::create_empty_particle(create_folder_override, create_asset_name);
				break;
			case CreateAssetType::MasterMaterial: {
				const char* domain_names[] = {"Default", "PostProcess", "Terrain", "Decal", "UI", "Particle"};
				result = AssetTemplates::create_mm_from_template(create_folder_override, create_asset_name, domain_names[create_mm_domain]);
				break;
			}
			case CreateAssetType::MaterialInstance:
				result = AssetTemplates::create_mi_from_master(create_folder_override, create_asset_name, create_mi_master_path);
				break;
			case CreateAssetType::Prefab:
				result = AssetTemplates::create_empty_prefab(create_folder_override, create_asset_name);
				break;
			default:
				break;
			}

			if (result) {
				sys_print(Info, "Created asset: %s\n", result->c_str());
			} else {
				sys_print(Warning, "Failed to create asset (already exists or write error)\n");
			}

			create_asset_type = CreateAssetType::None;
			ImGui::CloseCurrentPopup();
		}

		if (do_cancel) {
			create_asset_type = CreateAssetType::None;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void AssetBrowser::import_model_dialog() {
	auto glb_gamepath = OpenGlbFileDialog();
	if (!glb_gamepath)
		return;

	auto result = AssetTemplates::create_mis_for_glb(*glb_gamepath);
	if (result) {
		sys_print(Info, "Imported model: %s\n", result->c_str());
		set_selected(*glb_gamepath);
	} else {
		sys_print(Warning, "Failed to import model (already imported or write error): %s\n", glb_gamepath->c_str());
	}
}

// Separate call — caller should invoke this once per frame alongside imgui_draw().
// AssetInspectorPane::imgui_draw() opens its own Begin/End dockable window.
void AssetBrowser::imgui_draw_inspector() {
    if (inspector_pane)
        inspector_pane->imgui_draw(selected_resource);
}
#include "Render/DrawPublic.h"
#include "Render/IGraphicsDevice.h"

#include "Framework/MapUtil.h"
#include "Render/MaterialPublic.h"
#include "Render/MaterialLocal.h"

// ---------------------------------------------------------------------------
// ThumbnailManager — streaming thumbnail loader
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// DDS mip loader for Texture thumbnails (list view only)
// Redeclares minimal DDS structs locally to avoid depending on TextureDDS.cpp internals.
// ---------------------------------------------------------------------------
namespace {

struct ThumbDdsPF {
	uint32_t Size, Flags, FourCC, RGBBitCount;
	uint32_t RBitMask, GBitMask, BBitMask, ABitMask;
};
struct ThumbDdsHdr {
	uint32_t Size, Flags, Height, Width, PitchOrLinearSize, Depth, MipMapCount;
	uint32_t Reserved1[11];
	ThumbDdsPF ddspf;
	uint32_t Caps1, Caps2;
	uint32_t Reserved2[3];
};
static constexpr uint32_t kDDSF_FOURCC      = 0x00000004u;
static constexpr uint32_t kDDSF_MIPMAPCOUNT = 0x00020000u;

static Texture* load_dds_mip_for_thumb(const AssetOnDisk& asset) {
	auto f = FileSys::open_read_game(asset.filename);
	if (!f) return nullptr;
	int len = f->size();
	if (len < 4 + (int)sizeof(ThumbDdsHdr)) return nullptr;
	std::vector<uint8_t> buf(len);
	f->read(buf.data(), len);

	if (buf[0] != 'D' || buf[1] != 'D' || buf[2] != 'S' || buf[3] != ' ') return nullptr;

	const auto* hdr = reinterpret_cast<const ThumbDdsHdr*>(buf.data() + 4);
	const uint8_t* data = buf.data() + 4 + sizeof(ThumbDdsHdr);

	const uint32_t dxt1_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24);
	const uint32_t dxt5_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('5' << 24);
	const uint32_t bc4u_fourcc = 'B' | ('C' << 8) | ('4' << 16) | ('U' << 24);
	const uint32_t bc5u_fourcc = 'B' | ('C' << 8) | ('5' << 16) | ('U' << 24);

	using gtf = GraphicsTextureFormat;
	gtf fmt{};
	bool is_compressed = (hdr->ddspf.Flags & kDDSF_FOURCC) != 0;
	bool is_rgb8 = false;
	int  block_bytes = 0;

	if (is_compressed) {
		if      (hdr->ddspf.FourCC == dxt1_fourcc) { fmt = gtf::bc1; block_bytes = 8;  }
		else if (hdr->ddspf.FourCC == dxt5_fourcc) { fmt = gtf::bc3; block_bytes = 16; }
		else if (hdr->ddspf.FourCC == bc4u_fourcc) { fmt = gtf::bc4; block_bytes = 8;  }
		else if (hdr->ddspf.FourCC == bc5u_fourcc) { fmt = gtf::bc5; block_bytes = 16; }
		else return nullptr; // unsupported fourcc
	} else {
		if      (hdr->ddspf.RGBBitCount == 32) fmt = gtf::rgba8;
		else if (hdr->ddspf.RGBBitCount == 24) { fmt = gtf::rgba8; is_rgb8 = true; }
		else return nullptr;
	}

	int num_mips = 1;
	if (hdr->Flags & kDDSF_MIPMAPCOUNT) num_mips = (int)hdr->MipMapCount;

	// Skip mips until both dimensions are ≤ 64
	int mip_w = (int)hdr->Width, mip_h = (int)hdr->Height;
	for (int i = 0; i < num_mips - 1 && (mip_w > 64 || mip_h > 64); ++i) {
		int skip;
		if (is_compressed)
			skip = ((mip_w + 3) / 4) * ((mip_h + 3) / 4) * block_bytes;
		else
			skip = mip_w * mip_h * (is_rgb8 ? 3 : 4);
		if (data + skip > buf.data() + len) return nullptr;
		data += skip;
		mip_w = std::max(1, mip_w / 2);
		mip_h = std::max(1, mip_h / 2);
	}

	int upload_size;
	if (is_compressed)
		upload_size = ((mip_w + 3) / 4) * ((mip_h + 3) / 4) * block_bytes;
	else
		upload_size = mip_w * mip_h * (is_rgb8 ? 3 : 4);

	if (data + upload_size > buf.data() + len) return nullptr;

	// Widen RGB8 → RGBA8 (SDL3/GPU backends have no 24-bit upload)
	std::vector<uint8_t> widened;
	const void* upload_data = data;
	if (is_rgb8) {
		int pixels = mip_w * mip_h;
		widened.resize(pixels * 4);
		for (int i = 0; i < pixels; ++i) {
			widened[i * 4 + 0] = data[i * 3 + 0];
			widened[i * 4 + 1] = data[i * 3 + 1];
			widened[i * 4 + 2] = data[i * 3 + 2];
			widened[i * 4 + 3] = 255;
		}
		upload_size = pixels * 4;
		upload_data = widened.data();
	}

	CreateTextureArgs args;
	args.width        = mip_w;
	args.height       = mip_h;
	args.num_mip_maps = 1;
	args.format       = fmt;
	args.sampler_type = GraphicsSamplerType::NearestDefault;
	IGraphicsTexture* gpu = gfx().create_texture(args);
	gpu->sub_image_upload(0, 0, 0, mip_w, mip_h, upload_size, upload_data);

	Texture* t = Texture::install_system("__dds_thumb/" + asset.filename);
	t->update_specs_ptr(gpu);
	return t;
}

} // anonymous namespace

bool ThumbnailManager::supports_thumbnail(const AssetOnDisk& asset) {
	if (!asset.type) return false;
	auto* cls = asset.type->get_asset_class_type();
	return cls == &Model::StaticType || cls == &MaterialInstance::StaticType;
}

bool ThumbnailManager::supports_image_thumb(const AssetOnDisk& asset) {
	if (!asset.type) return false;
	return asset.type->get_asset_class_type() == &Texture::StaticType;
}

Texture* ThumbnailManager::get_thumbnail(const AssetOnDisk& asset) {
	if (!supports_thumbnail(asset) && !supports_image_thumb(asset)) return nullptr;

	auto it = entries.find(asset.filename);
	if (it == entries.end()) {
		Entry e;
		e.asset = asset;
		e.last_seen_frame = frame_counter;
		e.is_tex_entry = supports_image_thumb(asset);
		if (!e.is_tex_entry) {
			string hashed = StringUtils::alphanumeric_hash(asset.filename);
			e.thumb_path = ".thumbnails/" + hashed + ".png";
		}
		entries.emplace(asset.filename, std::move(e));
		return nullptr;
	}

	Entry& e = it->second;
	e.last_seen_frame = frame_counter; // bump priority — user can see this item
	return (e.state == EntryState::Loaded) ? e.tex : nullptr;
}

void ThumbnailManager::process_render(Entry& e) {
	ASSERT(e.state == EntryState::Queued);

	// DDS texture thumbnails are loaded directly from source — no GPU render needed.
	if (e.is_tex_entry) {
		e.state = EntryState::NeedLoad;
		return;
	}

	auto* asset_class = e.asset.type->get_asset_class_type();

	auto model_file     = FileSys::open_read_game(e.asset.filename);
	auto thumbnail_file = FileSys::open_read_game(e.thumb_path);

	bool needs_render = e.force_rerender || !thumbnail_file || !model_file ||
	                    model_file->get_timestamp() > thumbnail_file->get_timestamp();
	e.force_rerender = false;
	thumbnail_file.reset();
	model_file.reset();

	if (needs_render) {
		Model* the_model = nullptr;
		MaterialInstance* override_mat = nullptr;

		if (asset_class == &Model::StaticType) {
			the_model = Model::load(e.asset.filename);
		} else {
			auto mat = MaterialInstance::load(e.asset.filename);
			if (mat && mat->impl && mat->impl->get_master_impl() &&
			    mat->impl->get_master_impl()->usage == MaterialUsage::Default) {
				override_mat = mat;
				the_model = Model::load("sphere.cmdl");
			}
		}

		if (!the_model) {
			e.state = EntryState::Failed;
			return;
		}

		idraw->editor_render_thumbnail_for(the_model, override_mat, 128, 128,
		                                   FileSys::get_full_path_from_game_path(e.thumb_path));
	}

	e.state = EntryState::NeedLoad;
}

void ThumbnailManager::process_load(Entry& e) {
	ASSERT(e.state == EntryState::NeedLoad);

	if (e.is_tex_entry) {
		if (e.tex) {
			// Already loaded on a previous tick — nothing to do.
			e.state = EntryState::Loaded;
		} else {
			Texture* t = load_dds_mip_for_thumb(e.asset);
			if (t && t->gpu_ptr) {
				e.tex   = t;
				e.state = EntryState::Loaded;
			} else {
				e.state = EntryState::Failed;
			}
		}
		return;
	}

	if (e.tex) {
		// Already has a GPU texture from a prior load — reload it in-place so the
		// Texture* address stays stable and we avoid a duplicate install_system_asset.
		g_assets.reload(e.tex);
		e.state = e.tex->gpu_ptr ? EntryState::Loaded : EntryState::Failed;
	} else {
		// First load — force_load_for_ui sets force_nearest=true which skips generate_mipmaps.
		Texture* t = Texture::force_load_for_ui(e.thumb_path);
		if (t && t->gpu_ptr) {
			e.tex   = t;
			e.state = EntryState::Loaded;
		} else {
			e.state = EntryState::Failed;
		}
	}
}

void ThumbnailManager::invalidate_thumbnail(const std::string& asset_gamepath) {
	auto it = entries.find(asset_gamepath);
	if (it == entries.end())
		return;
	Entry& e = it->second;
	// Only act on entries that have already been rendered or are in-flight for load.
	// Queued/Failed entries will naturally re-process and pick up any changes.
	if (e.state == EntryState::Loaded || e.state == EntryState::NeedLoad) {
		e.state = EntryState::Queued;
		e.force_rerender = true;
	}
}

void ThumbnailManager::tick() {
	++frame_counter;

	// Find the highest-priority Queued entry (needs render check + optional GPU render).
	// Find the top MAX_LOADS highest-priority NeedLoad entries (cheap: disk read + upload).
	static constexpr int MAX_LOADS = 2;

	Entry* best_render = nullptr;
	Entry* best_load[MAX_LOADS] = {};
	int load_count = 0;

	for (auto& [path, e] : entries) {
		if (e.state == EntryState::Queued) {
			if (!best_render || e.last_seen_frame > best_render->last_seen_frame)
				best_render = &e;
		} else if (e.state == EntryState::NeedLoad) {
			if (load_count < MAX_LOADS) {
				best_load[load_count++] = &e;
			} else {
				// Replace the worst slot if this entry is more urgent
				int worst = 0;
				for (int i = 1; i < MAX_LOADS; i++)
					if (best_load[i]->last_seen_frame < best_load[worst]->last_seen_frame)
						worst = i;
				if (e.last_seen_frame > best_load[worst]->last_seen_frame)
					best_load[worst] = &e;
			}
		}
	}

	if (best_render)
		process_render(*best_render);

	for (int i = 0; i < load_count; i++)
		process_load(*best_load[i]);
}
#endif
