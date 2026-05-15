#pragma once
// Editor-only: walks the level for NavMeshVolumeComponents + nav_static MeshComponents,
// builds a Recast/Detour navmesh and hands the result to RuntimeNavManager::inst.
//
// @docs [[navigation#baker]]

#ifdef EDITOR_BUILD

class NavMeshBaker
{
public:
	// Bakes the level currently held by `eng->get_level()`. Returns true on success.
	// Logs errors via sys_print when the level is missing required components
	// (NavMeshSettingsComponent, at least one NavMeshVolumeComponent).
	bool bake_current_level();
};

#endif // EDITOR_BUILD
