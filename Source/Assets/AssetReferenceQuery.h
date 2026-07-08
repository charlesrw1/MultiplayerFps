#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>

class AssetMetadata;

// One discovered reference edge: an asset found by a query, with how many
// times it matched and its resolved asset type (null if not a recognized
// registered asset type).
struct AssetRefHit
{
	std::string game_path;
	const AssetMetadata* type = nullptr;
	int count = 0;
};

namespace AssetReferenceQuery
{
	// Assets whose content references `asset_gamepath` (who points at me).
	std::vector<AssetRefHit> find_backward_references(const std::string& asset_gamepath);

	// Assets referenced by `asset_gamepath`'s own file content (what I point at).
	std::vector<AssetRefHit> find_forward_references(const std::string& asset_gamepath);

	// Depth-limited BFS over find_backward_references/find_forward_references,
	// deduped by game path. Each returned hit's count is from the edge that
	// first discovered it.
	std::vector<AssetRefHit> find_transitive_references(const std::string& asset_gamepath, bool backward, int max_depth = 8);
}

#endif
