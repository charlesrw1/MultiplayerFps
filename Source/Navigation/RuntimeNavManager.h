#pragma once
// Runtime owner of the loaded Detour navmesh. Singleton, lifecycle parallels RenderGiManager::inst.
// All callers go through this class so detour types stay out of headers — keeps the path open for
// swapping dtNavMesh for dtTileCache in v2 without touching call sites.
//
// @docs [[navigation#runtime-nav-manager]]

#include "glm/glm.hpp"
#include <memory>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

// Custom deleters — Detour uses its own allocator (dtAlloc/dtFree).
struct DtNavMeshDeleter { void operator()(dtNavMesh* p) const noexcept; };
struct DtNavMeshQueryDeleter { void operator()(dtNavMeshQuery* p) const noexcept; };
using DtNavMeshPtr      = std::unique_ptr<dtNavMesh, DtNavMeshDeleter>;
using DtNavMeshQueryPtr = std::unique_ptr<dtNavMeshQuery, DtNavMeshQueryDeleter>;

struct NavQueryFilter
{
	// v1: empty. v2 will hold per-area cost overrides. Carried through find_path so v2 is a
	// pure addition with no API churn for v1 callers.
};

struct NavPathRequestHandle
{
	uint32_t id = 0;
	bool valid() const { return id != 0; }
};

class RuntimeNavManager
{
public:
	static RuntimeNavManager* inst;

	RuntimeNavManager();
	~RuntimeNavManager();

	// Hand off ownership of a freshly loaded/baked Detour mesh. Replaces any prior mesh.
	// The manager will build a dtNavMeshQuery on top of it.
	void set_navmesh_from_loading(DtNavMeshPtr mesh);

	// Drop any loaded mesh — used by Level::on_scene_exit teardown.
	void clear();

	bool has_navmesh() const { return navmesh != nullptr; }
	dtNavMesh* get_navmesh() { return navmesh.get(); }
	const dtNavMesh* get_navmesh() const { return navmesh.get(); }

	// Synchronous A* via dtNavMeshQuery::findPath + findStraightPath.
	// `filter` may be nullptr (default cost). Returns true on success and writes a corner string
	// (start..end) into out_corners.
	bool find_path(glm::vec3 start, glm::vec3 end, std::vector<glm::vec3>& out_corners,
				   const NavQueryFilter* filter = nullptr);

	// Snap a world position to the nearest poly on the navmesh within the default extents.
	bool nearest_point_on_navmesh(glm::vec3 in, glm::vec3& out) const;

	// v1 stub: forwards to find_path synchronously. v2 will dispatch to a worker pool.
	NavPathRequestHandle find_path_async(glm::vec3 start, glm::vec3 end,
										 std::vector<glm::vec3>& out_corners,
										 const NavQueryFilter* filter = nullptr);

private:
	DtNavMeshPtr navmesh;
	DtNavMeshQueryPtr query;
};
