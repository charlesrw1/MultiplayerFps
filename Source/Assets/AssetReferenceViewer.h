#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include "Assets/AssetReferenceQuery.h"

// Right-click an asset in the browser -> "View References..." to see what
// references it (or what it references), optionally walked transitively
// (e.g. Model -> MaterialInstance -> master Material -> Textures).
class AssetReferenceViewer
{
public:
	static AssetReferenceViewer& get();

	void open_for(const std::string& asset_gamepath);
	void imgui_draw();

	bool is_open = false;

private:
	void navigate_to(const std::string& gamepath);
	void run_query();

	std::string current_asset;
	std::vector<std::string> back_stack;
	std::vector<std::string> forward_stack;

	bool direction_backward = true; // true: "Referenced By", false: "References"
	bool transitive = false;
	uint32_t type_filter_mask = ~0u;
	char text_filter[256] = {};

	std::vector<AssetRefHit> results;
	bool dirty = true;
};

#endif
