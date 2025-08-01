#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Render/Texture.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/ConsoleCmdGroup.h"

class ThumbnailManager {
public:
	Texture* get_thumbnail(const AssetOnDisk& asset);
private:
	std::unordered_map<std::string, Texture*> cache;
};

class AssetBrowser
{
public:
	static AssetBrowser* inst;
	AssetBrowser();

	void imgui_draw();

	void clear_filter() {
		filter_type_mask = -1;
	}
	void filter_all() {
		filter_type_mask = 0;
	}
	void unset_filter(int type) {
		filter_type_mask |= (uint32_t)type;
	}
	void set_filter(int type) {
		filter_type_mask &= ~((uint32_t)type);
	}
	bool should_type_show(int type) const {
		return filter_type_mask & (uint32_t)type;
	}
	void set_selected(const std::string& path);
	AssetFilesystemNode* find_node_for_asset(const std::string& path) const;

	void draw_browser_grid();

	enum class Mode
	{
		Rows,
		Grid,
	};

	bool force_focus = false;
	bool show_filter_type_options = true;
	Mode mode = Mode::Grid;
	char asset_name_filter[256];
	bool filter_match_case = false;
	uint32_t filter_type_mask = -1;

	AssetOnDisk selected_resource;
	bool double_clicked_selected = false;

	Texture* folder_open{};
	Texture* folder_closed{};

	bool using_grid = false;

	AssetOnDisk drag_drop;
	std::string all_lower_cast_filter_name;

	uptr<ConsoleCmdGroup> commands;

	ThumbnailManager thumbnails;
};

#endif