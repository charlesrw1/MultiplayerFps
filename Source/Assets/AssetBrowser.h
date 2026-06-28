#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Render/Texture.h"

#include "Framework/ConsoleCmdGroup.h"

class AssetInspectorPane;

// Streaming thumbnail manager. Spreads expensive work (GPU render + disk load) across
// frames so the UI never stutters. Single-threaded; call tick() once per frame.
class ThumbnailManager
{
public:
	// Returns true if this asset type can have a thumbnail (model/material — shown in grid AND list).
	static bool supports_thumbnail(const AssetOnDisk& asset);
	// Returns true if this asset type has an image thumbnail (Texture — list view only).
	static bool supports_image_thumb(const AssetOnDisk& asset);

	// Returns the cached Texture* if ready, nullptr if still loading.
	// Calling this marks the asset as visible this frame (raises priority).
	Texture* get_thumbnail(const AssetOnDisk& asset);

	// Advance the queue: renders up to 1 thumbnail to disk, loads up to 2 from disk.
	// Must be called once per frame AFTER all get_thumbnail calls for that frame.
	void tick();

	// Mark the thumbnail for asset_gamepath as stale so it gets re-rendered next time visible.
	// Call after a Model or MaterialInstance asset is reloaded.
	void invalidate_thumbnail(const std::string& asset_gamepath);

private:
	enum class EntryState { Queued, NeedLoad, Loaded, Failed };

	struct Entry {
		AssetOnDisk asset;
		std::string thumb_path;
		EntryState state = EntryState::Queued;
		Texture* tex = nullptr;
		int last_seen_frame = 0;
		bool force_rerender = false; // skip freshness check on next process_render
		bool is_tex_entry = false;   // DDS image thumbnail — skip GPU render step
	};

	// Process one Queued entry: check freshness and render if needed, then advance to NeedLoad.
	void process_render(Entry& e);
	// Process one NeedLoad entry: load PNG from disk + upload to GPU, advance to Loaded/Failed.
	void process_load(Entry& e);

	std::unordered_map<std::string, Entry> entries;
	int frame_counter = 0;
};

class AssetBrowser
{
public:
	static AssetBrowser* inst;
	AssetBrowser();

	void imgui_draw();
	void imgui_draw_inspector(); // separate dockable "Asset Inspector" window

	void clear_filter() { filter_type_mask = -1; }
	void filter_all() { filter_type_mask = 0; }
	void unset_filter(int type) { filter_type_mask |= (uint32_t)type; }
	void set_filter(int type) { filter_type_mask &= ~((uint32_t)type); }
	bool should_type_show(int type) const { return filter_type_mask & (uint32_t)type; }
	void set_selected(const std::string& path);
	AssetFilesystemNode* find_node_for_asset(const std::string& path) const;

	void draw_browser_grid();

	enum class Mode
	{
		Rows,
		Grid,
	};

	bool big_thumbnail = false;

	bool force_focus = false;
	bool show_filter_type_options = true;
	Mode mode = Mode::Grid;
	char asset_name_filter[256];
	bool filter_match_case = false;
	uint32_t filter_type_mask = -1;

	AssetOnDisk selected_resource;
	bool double_clicked_selected = false;

	std::string selected_folder;
	float left_panel_width = 200.0f;
	uptr<AssetInspectorPane> inspector_pane;

	Texture* folder_open{};
	Texture* folder_closed{};

	bool using_grid = false;

	AssetOnDisk drag_drop;
	std::string all_lower_cast_filter_name;

	uptr<ConsoleCmdGroup> commands;

	ThumbnailManager thumbnails;

	enum class CreateAssetType { None, Map, Particle, MasterMaterial, MaterialInstance };
	CreateAssetType create_asset_type = CreateAssetType::None;
	char create_asset_name[128] = {};
	int create_mm_domain = 0;
	std::string create_mi_master_path;
	std::string create_folder_override;
	void draw_create_asset_popup();
};

#endif