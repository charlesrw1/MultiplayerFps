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
#include "EngineSystemCommands.h"
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
					auto assetType = type->get_asset_class_type();
					if (assetType) {
						auto cmd = std::make_unique<OpenEditorToolCommand>(*assetType, asset.filename, true);
						Cmd_Manager::inst->append_cmd(std::move(cmd));
					}
					else {
						sys_print(Warning, "AssetBrowser: Asset is not a standard IAsset, can't edit.\n");
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

// too much of a brainlet do the dumb thing
void fill_big_vector(std::vector<AssetFilesystemNode*>& nodes, AssetFilesystemNode* root) {
	if (!root)
		return;
	auto recurse = [](auto&& self, AssetFilesystemNode* n, std::vector<AssetFilesystemNode*>& nodes) -> void {
		if (!n->is_folder()) {
			assert(n->children.empty());
			nodes.push_back(n);
		}
		else {
			for (auto c : n->sorted_list) {
				self(self, c, nodes);
			}
		}
	};
	recurse(recurse, root, nodes);
}
#include "Render/Model.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"

bool ImageButtonWithOverlayText(ImTextureID texture, ImVec2 size, const char* label)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 label_pos = pos;

	// Invisible button to handle interaction
	ImGui::InvisibleButton(label, size);
	bool hovered = ImGui::IsItemHovered();
	bool pressed = ImGui::IsItemActive() && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

	// Draw the image
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddImage(texture, pos, ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 1), ImVec2(1, 0));

	// Optional highlight on hover
	if (hovered)
	{
		draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 50));
	}

	// Draw wrapped text over image
	float wrap_width = size.x - 10.0f; // small padding
	ImVec2 text_pos = ImVec2(pos.x + 5.0f, pos.y + 5.0f); // small margin
	ImGui::PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
	ImGui::PushTextWrapPos(text_pos.x + wrap_width);
	auto shadow_pos = text_pos + ImVec2(1, 1);
	draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), shadow_pos, IM_COL32(0, 0, 0, 255), label, nullptr, wrap_width);
	draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos, IM_COL32(255, 255, 255, 255), label, nullptr, wrap_width);

	ImGui::PopTextWrapPos();
	ImGui::PopClipRect();

	return pressed;
}

void AssetBrowser::draw_browser_grid() {
	const int SIZE_PER = 80;
	auto win_size = ImGui::GetWindowSize();
	int boxes = win_size.x / SIZE_PER;
	boxes = std::max(boxes, 1);

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY ;


	std::vector<AssetFilesystemNode*> items;
	fill_big_vector(items, AssetRegistrySystem::get().get_root_files());

	const int name_filter_len = strlen(asset_name_filter);
	if (ImGui::BeginTable("Browser", boxes, ent_list_flags))
	{
		for (int i = 0; i < boxes; i++) {
			ImGui::TableSetupColumn("##blah", ImGuiTableColumnFlags_WidthStretch);
		}

		int cur_row = 0;
		int cur_col = 0;

		for (auto c : items) {
			Texture* t = thumbnails.get_thumbnail(c->asset);
			if (!t) 
				continue;
			{
				auto& asset = c->asset;
				if (!filter_match_case && name_filter_len > 0) {
					std::string path = asset.filename;
					for (int i = 0; i < path.size(); i++) path[i] = tolower(path[i]);
					if (path.find(all_lower_cast_filter_name, 0) == std::string::npos)
						continue;
				}
				else if (name_filter_len > 0) {
					if (asset.filename.find(asset_name_filter) == std::string::npos)
						continue;
				}
			}
		
			ImGui::PushID(c);
			if (cur_col == 0) {
				ImGui::TableNextRow();
			}
			ImGui::TableNextColumn();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		//	ImGui::ImageButton(ImTextureID(uint64_t(t->gl_id)), ImVec2(64, 64),ImVec2(0, 1), ImVec2(1, 0));
			string only_filename = c->asset.filename;
			StringUtils::get_filename(only_filename);
			ImageButtonWithOverlayText(ImTextureID(uint64_t(t->gl_id)), ImVec2(64, 64), only_filename.c_str());
			ImGui::PopStyleColor();
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				drag_drop = c->asset;
				auto ptr = &drag_drop;
				ImGui::SetDragDropPayload("AssetBrowserDragDrop", &ptr, sizeof(AssetOnDisk*));

				ImGui::TextColored(color32_to_imvec4(c->asset.type->get_browser_color()), "%s", c->asset.type->get_type_name().c_str());
				ImGui::Text("Asset: %s", c->asset.filename.c_str());

				ImGui::EndDragDropSource();
			}

			cur_col += 1;
			if (cur_col >= boxes) {
				cur_col = 0;
				cur_row += 1;
			}
			ImGui::PopID();
		}


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
	ImGui::Checkbox("Grid", &using_grid);

	if (!match_case) {
		all_lower_cast_filter_name = asset_name_filter;
		for (int i = 0; i < name_filter_len; i++)
			all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
	}
	if (using_grid) {
		draw_browser_grid();
	}
	else {
		draw_browser_tree_view(this);
	}
	ImGui::End();

}
#include "Render/DrawPublic.h"

#include "Framework/MapUtil.h"
Texture* ThumbnailManager::get_thumbnail(const AssetOnDisk& asset)
{
	if (asset.type->get_asset_class_type() != &Model::StaticType)
		return nullptr;

	if (MapUtil::contains(cache, asset.filename)) {
		return cache[asset.filename];
	}
	// load it
	auto model_file = FileSys::open_read_game(asset.filename);
	if (!model_file)
		return nullptr;

	string hashed = StringUtils::alphanumeric_hash(asset.filename);
	string thumnail_path = ".thumbnails/" + hashed + ".png";
	auto thumbnail_file = FileSys::open_read_game(thumnail_path);

	Texture* out_t = nullptr;
	if (!thumbnail_file || model_file->get_timestamp() > thumbnail_file->get_timestamp()) {
		thumbnail_file.reset();
		model_file.reset();

		Model* the_model = Model::load(asset.filename);
		if (the_model) {
			// hmm..
			idraw->editor_render_thumbnail_for(the_model, 64, 64, FileSys::get_full_path_from_game_path(thumnail_path));
		}
		out_t = Texture::load(thumnail_path);
		//out_t = Texture::load("eng/icon/_nearest/skylight.png");
	}
	else {
		thumbnail_file.reset();
		model_file.reset();
		out_t = Texture::load(thumnail_path);
	}
	cache[asset.filename] = out_t;
	return out_t;
}
#endif
