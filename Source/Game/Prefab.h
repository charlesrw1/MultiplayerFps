#pragma once
#include <string>

// PrefabFile — Simple file I/O for .tprefab files
// Not an IAsset — just string-based path and serialization text
class PrefabFile
{
public:
	// Load serialized entity text from a .tprefab file
	// Returns empty string on failure
	static std::string load_text(const std::string& game_relative_path);

	// Save serialized entity text to a .tprefab file
	// Returns true on success
	static bool save_text(const std::string& game_relative_path, const std::string& text);
};

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Framework/Util.h"

// AssetMetadata for .tprefab files in the asset browser
class PrefabAssetMetadata : public AssetMetadata
{
public:
	PrefabAssetMetadata() { extensions.push_back("tprefab"); }
	std::string get_type_name() const override { return "Prefab"; }
	Color32 get_browser_color() const override { return {255, 200, 100}; }
};

#endif // EDITOR_BUILD
