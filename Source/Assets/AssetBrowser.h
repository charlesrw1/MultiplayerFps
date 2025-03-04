#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Render/Texture.h"
#include "Game/SerializePtrHelpers.h"
class AssetBrowser
{
public:
	void init();
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


	AssetOnDisk drag_drop;
	std::string all_lower_cast_filter_name;
};

extern AssetBrowser global_asset_browser;
#endif