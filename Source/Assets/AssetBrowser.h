#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Render/Texture.h"

#include "Framework/ConsoleCmdGroup.h"
#include "Framework/MyImguiLib.h"

class AssetInspectorPane;

// Streaming thumbnail manager. Spreads expensive work (GPU render + disk load) across
// frames so the UI never stutters. Single-threaded; call tick() once per frame.
class ThumbnailManager
{
public:
	// Returns true if this asset type can have a rendered thumbnail (model/material/prefab).
	static bool supports_thumbnail(const AssetOnDisk& asset);
	// Returns true if this asset type has an image thumbnail loaded directly from disk (Texture).
	static bool supports_image_thumb(const AssetOnDisk& asset);
	// True if get_thumbnail() can ever return a real texture for this asset (vs. always
	// falling back to the tinted document icon).
	static bool supports_any_thumb(const AssetOnDisk& asset) { return supports_thumbnail(asset) || supports_image_thumb(asset); }

	// Returns the cached Texture* if ready, nullptr if still loading.
	// Calling this marks the asset as visible this frame (raises priority).
	Texture* get_thumbnail(const AssetOnDisk& asset);

	// True once this asset has been attempted and permanently has nothing to render (e.g. a
	// Prefab with no MeshComponent anywhere in it). Callers should fall back to the generic
	// tinted document icon instead of the "still loading" placeholder for these.
	bool thumbnail_failed(const AssetOnDisk& asset) const;

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

	// Type filter is single-select: -1 shows every type ("All"), otherwise only
	// assets whose AssetMetadata::self_index matches are shown.
	void clear_filter() { filter_type_index = -1; }
	void set_filter(int type_index) { filter_type_index = type_index; }
	bool should_type_show(int type_index) const { return filter_type_index < 0 || filter_type_index == type_index; }
	void set_selected(const std::string& path);
	AssetFilesystemNode* find_node_for_asset(const std::string& path) const;

	// "Ping" flash duration (seconds) for the yellow highlight shown on the folder/asset
	// navigated to via set_selected(), mirroring Unity's "ping" find-in-browser feedback.
	static constexpr float PING_DURATION = 0.8f;
	float ping_timer = 0.0f;

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
	int filter_type_index = -1;

	AssetOnDisk selected_resource;
	bool double_clicked_selected = false;

	std::string selected_folder;
	// When a search filter is active, whether it searches every asset ("All") or is
	// scoped to selected_folder and its subdirectories ("Folder"). Only used while
	// asset_name_filter is non-empty; with no filter, selected_folder always shows
	// just its direct children (Unity-style folder browsing).
	enum class SearchScope { All, Folder };
	SearchScope search_scope = SearchScope::All;
	float left_panel_width = 200.0f;
	uptr<AssetInspectorPane> inspector_pane;

	Texture* folder_open{};
	Texture* folder_closed{};
	Texture* import_model_icon{};
	Texture* filter_icon{};
	Texture* document_icon{}; // fallback thumbnail (tinted by asset type color) for types with no real thumbnail
	Texture* folder_big{}; // folder tile icon used in grid view

	bool using_grid = false;

	AssetOnDisk drag_drop;
	std::string all_lower_cast_filter_name;

	uptr<ConsoleCmdGroup> commands;

	ThumbnailManager thumbnails;

	enum class CreateAssetType { None, Map, Particle, MasterMaterial, MaterialInstance, Prefab, ScriptableObject };
	CreateAssetType create_asset_type = CreateAssetType::None;
	char create_asset_name[128] = {};
	int create_mm_domain = 0;
	std::string create_mi_master_path;
	std::string create_folder_override;
	// Concrete ScriptableObject subclass classname chosen from the "Scriptable Object" submenu.
	std::string create_sobj_classname;
	void draw_create_asset_popup();

	// Shared path-prompt for the right-click context menu's Duplicate / Make Prefab Using... /
	// actions. Drawn unconditionally once per frame (see imgui_draw()).
	FolderNamePopup path_popup;

	// Opens a native file picker rooted at the project data dir, filtered to .glb,
	// then writes a .mis sidecar for the picked model so it shows up as an importable asset.
	void import_model_dialog();
};

#endif