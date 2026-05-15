#include "RuntimeNavManager.h"

#include "recastnavigation/DetourNavMesh.h"
#include "recastnavigation/DetourNavMeshQuery.h"
#include "recastnavigation/DetourStatus.h"

#include "Debug.h"

RuntimeNavManager* RuntimeNavManager::inst = nullptr;

namespace {
constexpr int kQueryMaxNodes = 2048;
constexpr int kMaxPathPolys  = 256;
constexpr int kMaxCorners    = 64;
const float kQueryHalfExtents[3] = {2.f, 4.f, 2.f}; // default search radius around (x,y,z)
}

RuntimeNavManager::RuntimeNavManager() = default;
RuntimeNavManager::~RuntimeNavManager() = default;

void DtNavMeshDeleter::operator()(dtNavMesh* p) const noexcept { if (p) dtFreeNavMesh(p); }
void DtNavMeshQueryDeleter::operator()(dtNavMeshQuery* p) const noexcept { if (p) dtFreeNavMeshQuery(p); }

void RuntimeNavManager::set_navmesh_from_loading(DtNavMeshPtr mesh) {
	ASSERT(mesh != nullptr);
	navmesh = std::move(mesh);
	query.reset(dtAllocNavMeshQuery());
	ASSERT(query);
	dtStatus s = query->init(navmesh.get(), kQueryMaxNodes);
	if (dtStatusFailed(s)) {
		sys_print(Warning, "RuntimeNavManager: dtNavMeshQuery::init failed\n");
		query.reset();
		navmesh.reset();
	}
}

void RuntimeNavManager::clear() {
	query.reset();
	navmesh.reset();
}

bool RuntimeNavManager::nearest_point_on_navmesh(glm::vec3 in, glm::vec3& out) const {
	if (!query)
		return false;
	dtQueryFilter filter;
	float in_arr[3]   = {in.x, in.y, in.z};
	float out_arr[3]  = {0, 0, 0};
	dtPolyRef ref     = 0;
	dtStatus s        = query->findNearestPoly(in_arr, kQueryHalfExtents, &filter, &ref, out_arr);
	if (dtStatusFailed(s) || ref == 0)
		return false;
	out = glm::vec3(out_arr[0], out_arr[1], out_arr[2]);
	return true;
}

bool RuntimeNavManager::find_path(glm::vec3 start, glm::vec3 end, std::vector<glm::vec3>& out_corners,
								  const NavQueryFilter* /*user_filter*/) {
	out_corners.clear();
	if (!query)
		return false;

	dtQueryFilter filter;
	float start_arr[3] = {start.x, start.y, start.z};
	float end_arr[3]   = {end.x, end.y, end.z};
	float start_snap[3];
	float end_snap[3];
	dtPolyRef start_ref = 0;
	dtPolyRef end_ref   = 0;

	if (dtStatusFailed(query->findNearestPoly(start_arr, kQueryHalfExtents, &filter, &start_ref, start_snap))
		|| start_ref == 0)
		return false;
	if (dtStatusFailed(query->findNearestPoly(end_arr, kQueryHalfExtents, &filter, &end_ref, end_snap))
		|| end_ref == 0)
		return false;

	dtPolyRef path[kMaxPathPolys];
	int path_count = 0;
	dtStatus s     = query->findPath(start_ref, end_ref, start_snap, end_snap, &filter, path, &path_count,
									 kMaxPathPolys);
	if (dtStatusFailed(s) || path_count == 0)
		return false;

	float corners[3 * kMaxCorners];
	unsigned char corner_flags[kMaxCorners];
	dtPolyRef corner_polys[kMaxCorners];
	int corner_count = 0;
	s = query->findStraightPath(start_snap, end_snap, path, path_count, corners, corner_flags, corner_polys,
								&corner_count, kMaxCorners);
	if (dtStatusFailed(s) || corner_count == 0)
		return false;

	out_corners.reserve(corner_count);
	for (int i = 0; i < corner_count; i++) {
		out_corners.emplace_back(corners[i * 3 + 0], corners[i * 3 + 1], corners[i * 3 + 2]);
	}
	return true;
}

NavPathRequestHandle RuntimeNavManager::find_path_async(glm::vec3 start, glm::vec3 end,
														std::vector<glm::vec3>& out_corners,
														const NavQueryFilter* filter) {
	// v1: synchronous fallback. Caller may poll/use immediately. Handle reserved for v2.
	NavPathRequestHandle h;
	h.id = find_path(start, end, out_corners, filter) ? 1u : 0u;
	return h;
}
