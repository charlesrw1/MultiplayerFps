#pragma once
// Static façade for navmesh scene I/O + editor baking. Mirror of GameSceneGiUtil.
//
// @docs [[navigation#level-nav-util]]

#include <string>

class LevelNavUtil
{
public:
	// Open `<map>.navmesh`, parse blob, hand the resulting Detour mesh to RuntimeNavManager::inst.
	// No-op (and no warning above debug) when the sidecar is absent — levels without baked nav stay queryable=false.
	static void on_scene_load_nav(const std::string& mapname);

	// Drop the loaded mesh. Paired with on_scene_load_nav from Level::end / scene reset.
	static void on_scene_exit();

	// Editor-only. Iterate every NavMeshVolumeComponent in the current level, build one combined dtNavMesh,
	// hand it to RuntimeNavManager::inst. Reads parameters from the single NavMeshSettingsComponent.
	static void bake_all_volumes();

	// Editor-only. Serialize the live nav mesh in RuntimeNavManager::inst to `<map>.navmesh`.
	static void save_to_disk();

	// Editor hot-reload flag — set when something invalidates the bake, mirrors GameSceneGiUtil::had_changes.
	static bool had_changes;
};
