#include "Physics.h"
#include <vector>
#include "GlmInclude.h"
#include "MathLib.h"
#include "Level.h"
#include "MeshBuilder.h"
#include "glad/glad.h"
#include "BVH.h"
#include "CoreTypes.h"

extern MeshBuilder phys_debug;
static MeshBuilder world_collision;
void DrawCollisionWorld(const Level* lvl)
{
	static std::string lvl_that_was_built;
	if (lvl_that_was_built != lvl->name) {
		world_collision.Begin();
		int basev = world_collision.GetBaseVertex();
		const Level::CollisionData& cd = lvl->collision_data;
		for (int i = 0; i < cd.vertex_list.size(); i++) {
			world_collision.AddVertex(MbVertex(cd.vertex_list[i], COLOR_RED));
		}
		for (int i = 0; i < cd.collision_tris.size(); i++) {
			const Level::CollisionTri& ct = cd.collision_tris[i];
			world_collision.AddLine(basev + ct.indicies[0], basev + ct.indicies[1]);
			world_collision.AddLine(basev + ct.indicies[1], basev + ct.indicies[2]);
			world_collision.AddLine(basev + ct.indicies[2], basev + ct.indicies[0]);
		
			glm::vec3 center = cd.vertex_list[ct.indicies[0]] + cd.vertex_list[ct.indicies[1]] + cd.vertex_list[ct.indicies[2]];
			center /= 3.f;
			world_collision.PushLine(center, center + ct.face_normal*0.25f, COLOR_BLUE);
		}

		for (int i = 0; i < lvl->static_geo_bvh.nodes.size(); i++)
		{
			const BVHNode& node = lvl->static_geo_bvh.nodes[i];
			//world_collision.PushLineBox(node.aabb.bmin, node.aabb.bmax, COLOR_PINK);
		}


		world_collision.End();
		lvl_that_was_built = lvl->name;
	}
	world_collision.Draw(GL_LINES);
}
void InitStaticGeoBvh(Level* level)
{
	std::vector<Bounds> bound_vec;
	Level::CollisionData& cd = level->collision_data;
	for (int i = 0; i < cd.collision_tris.size(); i++) {
		Level::CollisionTri& tri = cd.collision_tris[i];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = cd.vertex_list[tri.indicies[i]];
		Bounds b(corners[0]);
		b = bounds_union(b, corners[1]);
		b = bounds_union(b, corners[2]);
		b.bmin -= vec3(0.01);
		b.bmax += vec3(0.01);

		bound_vec.push_back(b);
	}

	float time_start = GetTime();
	level->static_geo_bvh = BVH::build(bound_vec, 1, BVH_SAH);
	printf("Built world bvh in %.2f seconds\n", (float)GetTime() - time_start);
}

template<typename Functor>
static void IntersectWorld(Functor&& do_intersect, const BVH& bvh, Bounds box)
{
	const BVHNode* stack[64];
	stack[0] = &bvh.nodes[0];

	int stack_count = 1;
	const BVHNode* node = nullptr;


	int node_check = 0;
	bool found_hit = false;
	while (stack_count > 0)
	{
		node = stack[--stack_count];

		if (node->count != BVH_BRANCH)
		{
			node_check++;
			int index_count = node->count;
			int index_start = node->left_node;
			for (int i = 0; i < index_count; i++) {

				//vec3 bary_temp = vec3(0);
				//vec3 N_temp;
				//float t_temp = -1;
				//phys_debug.PushLineBox(node->aabb.bmin,node->aabb.bmax,COLOR_CYAN);
				int index = bvh.indicies[index_start + i];
				bool should_exit = do_intersect(index);
				if (should_exit)
					return;
				//vec3 v0 = mesh->verticies[mesh->indicies[mesh_element_index]].position;
				//vec3 v1 = mesh->verticies[mesh->indicies[mesh_element_index + 1]].position;
				//vec3 v2 = mesh->verticies[mesh->indicies[mesh_element_index + 2]].position;
				//IntersectTriRay2(r, v0,
				//	v1,
				//	v2, t_temp, bary_temp);
				//bool res = t_temp > 0;


				//if (!res || t_temp > tmax || t_temp < tmin)
				//	continue;
				//tmax = t_temp;
				//t = t_temp;
				//tri_vert_start = mesh_element_index;
				//bary = vec3(bary_temp.z, bary_temp.x, bary_temp.y);
			}
			continue;
		}

		bool left_aabb = bvh.nodes[node->left_node].aabb.intersect(box);
		bool right_aabb = bvh.nodes[node->left_node + 1].aabb.intersect(box);

		if (left_aabb) {
			stack[stack_count++] = &bvh.nodes[node->left_node];
		}
		if (right_aabb) {
			stack[stack_count++] = &bvh.nodes[node->left_node + 1];
		}
	}

	//if (!std::isfinite(t)) {
	//	return false;
	//}
	//
	//si->point = r.at(t);
	//vec3 N = bary.u * mesh->verticies[mesh->indicies[tri_vert_start]].normal +
	//	bary.v * mesh->verticies[mesh->indicies[tri_vert_start + 1]].normal +
	//	bary.w * mesh->verticies[mesh->indicies[tri_vert_start + 2]].normal;
	//si->set_face_normal(r, normalize(N));
	//si->t = t;
	//return true;
}

bool SphereVsTriangle(const Level::CollisionData& cd, const Level::CollisionTri& tri, 
	glm::vec3 pos, float radius, GeomContact* out, bool doublesided)
{
	float dist = dot(pos, tri.face_normal) + tri.plane_offset;
	if (dist < 0 && !doublesided)
		return false;
	if (dist > radius)
		return false;

	vec3 corners[3];
	for (int i = 0; i < 3; i++)
		corners[i] = cd.vertex_list[tri.indicies[i]];

	vec3 pointontri = pos - tri.face_normal * dist;
	vec3 c0 = cross(pointontri - corners[0], corners[1] - corners[0]);
	vec3 c1 = cross(pointontri - corners[1], corners[2] - corners[1]);
	vec3 c2 = cross(pointontri - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, tri.face_normal) <= 0 && dot(c1, tri.face_normal) <= 0 && dot(c2, tri.face_normal) <= 0;

	float radius2 = radius * radius;
	vec3 p1 = closest_point_on_line(corners[0], corners[1], pos);
	vec3 v1 = pos - p1;
	bool intersects = dot(v1, v1) < radius2;

	vec3 p2 = closest_point_on_line(corners[1], corners[2], pos);
	vec3 v2 = pos - p2;
	intersects |= dot(v2, v2) < radius2;

	vec3 p3 = closest_point_on_line(corners[2], corners[0], pos);
	vec3 v3 = pos - p3;
	intersects |= dot(v3, v3) < radius2;

	if (!intersects && !inside)
		return false;

	vec3 best_point = pointontri;
	vec3 intersection_vec = pos - best_point;

	if (!inside)
	{
		vec3 d = pos - p1;
		float best_distsq = dot(d, d);
		best_point = p1;
		intersection_vec = d;

		d = pos - p2;
		float distsq = dot(d, d);
		if (distsq < best_distsq) {
			best_distsq = distsq;
			best_point = p2;
			intersection_vec = d;
		}

		d = pos - p3;
		distsq = dot(d, d);
		if (distsq < best_distsq) {
			best_distsq = distsq;
			best_point = p3;
			intersection_vec = d;
		}

	}
	float len = length(intersection_vec);
	if (abs(len) < 0.0000001)
		return false;


	vec3 intersect_normal = intersection_vec / len;
	float intersect_depth = radius - len;

	out->found = true;
	out->intersect_len = len;
	out->penetration_normal = intersect_normal;
	out->penetration_depth = intersect_depth;
	out->intersect_point = best_point;
	out->surf_normal = tri.face_normal;

	return true;
}

vec3 CapsuleGetReferencePoint(const Level::CollisionData& cd, const Level::CollisionTri& tri, 
	const Capsule& cap, vec3 line_plane_intersect)
{
	glm::vec3 corners[3];
	for (int i = 0; i < 3; i++)
		corners[i] = cd.vertex_list[tri.indicies[i]];

	// Determine if intersection point is inside triangle
	vec3 c0 = cross(line_plane_intersect - corners[0], corners[1] - corners[0]);
	vec3 c1 = cross(line_plane_intersect - corners[1], corners[2] - corners[1]);
	vec3 c2 = cross(line_plane_intersect - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, tri.face_normal) <= 0 && dot(c1, tri.face_normal) <= 0 && dot(c2, tri.face_normal) <= 0;

	vec3 reference_point = line_plane_intersect;

	// Find closest edge to reference point if its not
	if (!inside)
	{
		vec3 p1 = closest_point_on_line(corners[0], corners[1], line_plane_intersect);
		vec3 v1 = line_plane_intersect - p1;
		float distsq = dot(v1, v1);
		float best_distsq = distsq;
		reference_point = p1;

		vec3 p2 = closest_point_on_line(corners[1], corners[2], line_plane_intersect);
		vec3 v2 = line_plane_intersect - p2;
		distsq = dot(v2, v2);
		if (distsq < best_distsq) {
			best_distsq = distsq;
			reference_point = p2;
		}

		vec3 p3 = closest_point_on_line(corners[2], corners[0], line_plane_intersect);
		vec3 v3 = line_plane_intersect - p3;

		distsq = dot(v3, v3);
		if (distsq < best_distsq) {
			best_distsq = distsq;
			reference_point = p3;
		}
	}

	return reference_point;
}

bool CapsuleVsTriangle(const Level::CollisionData& cd, const Level::CollisionTri& tri, 
	const Capsule& cap, GeomContact* out)
{
	vec3 cap_normal = normalize(cap.tip - cap.base);
	vec3 line_end_ofs = cap_normal * cap.radius;
	vec3 A = cap.base + line_end_ofs;
	vec3 B = cap.tip - line_end_ofs;

	glm::vec3 corners[3];
	for (int i = 0; i < 3; i++)
		corners[i]=cd.vertex_list[tri.indicies[i]];

	float denom = dot(tri.face_normal, cap_normal);

	// Intersect capsule normal (line segment from base to tip) with triangle's plane
	if (abs(denom) > 0.15) {	// such a high epsilon to deal with weird issues
		float planedist = dot(tri.face_normal, cap.base) + tri.plane_offset;
		float time = -planedist / denom;
		vec3 line_plane_intersect = cap.base + cap_normal * time;

		vec3 reference_point = CapsuleGetReferencePoint(cd,tri, cap, line_plane_intersect);

		//phys_debug.AddSphere(line_plane_intersect, 0.1f, 8, 6, COLOR_BLUE);

		//phys_debug.AddSphere(reference_point, 0.1f, 8, 6, COLOR_RED);

		vec3 sphere_center = closest_point_on_line(A, B, reference_point);

		//phys_debug.AddSphere(sphere_center, 0.1f, 8, 6, COLOR_PINK);
		// Finish with a sphere/triangle test
		return SphereVsTriangle(cd, tri, sphere_center, cap.radius, out, false);
	}
	else
	{
		// has the assumption that the capsule is vertical, hacky code
		vec3 ref_high = corners[0];
		vec3 ref_low = corners[0];
		if (corners[1].y > ref_high.y)
			ref_high = corners[1];
		else if (corners[1].y < ref_low.y)
			ref_low = corners[1];
		if (corners[2].y > ref_high.y)
			ref_high = corners[2];
		else if (corners[2].y < ref_low.y)
			ref_low = corners[2];
		{
			vec3 sphere_center = closest_point_on_line(A, B, ref_high);

		//	phys_debug.AddSphere(sphere_center, 0.1f, 8, 6, COLOR_PINK);
			if (SphereVsTriangle(cd, tri, sphere_center, cap.radius, out, false))
				return true;
		}
		{
			vec3 sphere_center = closest_point_on_line(A, B, ref_low);

			//phys_debug.AddSphere(sphere_center, 0.1f, 8, 6, COLOR_PINK);
			return (SphereVsTriangle(cd, tri, sphere_center, cap.radius, out, false));
		}

	}
}
static void SphereVsAABB(vec3 center, float radius, Bounds aabb, GeomContact* out)
{
	glm::vec3 closest_point = glm::clamp(center, aabb.bmin, aabb.bmax);
	float len = glm::length(closest_point - center);
	if (len < radius) {
		out->found = false;
		return;
	}
	out->found = true;
	out->intersect_len = len;
	out->intersect_point = closest_point;
	out->penetration_depth = (radius - len);
	out->penetration_normal = (center-closest_point) / len;
	out->surf_normal = out->penetration_normal;
}


Bounds CapsuleToAABB(const Capsule& cap)
{
	vec3 normal = normalize(cap.tip - cap.base);
	vec3 centertip = cap.tip - normal * cap.radius;
	vec3 centerbase = cap.base + normal * cap.radius;
	return bounds_union(Bounds(centertip - vec3(cap.radius), centertip + vec3(cap.radius)),
		Bounds(centerbase - vec3(cap.radius), centerbase + vec3(cap.radius)));
}


void TraceCapsule(const Level* lvl, glm::vec3 pos, const Capsule& cap, GeomContact* out, bool closest)
{
	const Level::CollisionData& cd = lvl->collision_data;

	Capsule adjusted_cap = cap;
	adjusted_cap.base += pos;
	adjusted_cap.tip += pos;
	Bounds b2 = CapsuleToAABB(adjusted_cap);
	b2.bmin -= vec3(0.05);
	b2.bmax += vec3(0.05);

	float best_len_total = INFINITY;

	bool found_ground = false;
	GeomContact temp;

	auto capsule_intersect_functor = [&](int index) -> bool {
		ASSERT(index < cd.collision_tris.size());
		const Level::CollisionTri& tri = cd.collision_tris[index];
		bool res = CapsuleVsTriangle(cd, tri, adjusted_cap, &temp);
		if (res && temp.intersect_len < best_len_total) {
			best_len_total = temp.intersect_len;
			*out = temp;
			if (!closest)
				return true;
		}
		return false;
	};
	IntersectWorld(capsule_intersect_functor, lvl->static_geo_bvh, b2);

	out->touched_ground = found_ground;
}

void TraceSphere(const Level* level, glm::vec3 org, float radius, GeomContact* out, bool closest, bool double_sided)
{
	const Level::CollisionData& cd = level->collision_data;

	float best_len_total = INFINITY;

	GeomContact temp;
	auto sphere_intersect_functor = [&](int index)->bool {
		ASSERT(index < cd.collision_tris.size());
		const Level::CollisionTri& tri = cd.collision_tris[index];
		bool res = SphereVsTriangle(cd, tri, org, radius, &temp, double_sided);
		if (res && temp.intersect_len < best_len_total) {
			best_len_total = temp.intersect_len;
			*out = temp;
			if (!closest)
				return true;
		}
		return false;
	};
	Bounds sphere(vec3(-radius + org), vec3(radius + org));
	IntersectWorld(sphere_intersect_functor, level->static_geo_bvh, sphere);
}

void TraceAgainstLevel(const Level* lvl, GeomContact* out, PhysContainer obj, bool closest, bool double_sided)
{
	switch (obj.type)
	{
	case PhysContainer::CapsuleType:
		TraceCapsule(lvl, glm::vec3(0.f), obj.cap, out, closest);
		break;
	case PhysContainer::SphereType:
		TraceSphere(lvl, obj.sph.origin, obj.sph.radius, out, closest, double_sided);
		break;
	default:
		ASSERT(0);
	}
}

inline void IntersectTriRay2(const Ray& r, const vec3& v0, const vec3& v1, const vec3& v2, float& t_out, vec3& bary_out)
{
	const vec3 edge1 = v1 - v0;
	const vec3 edge2 = v2 - v0;
	const vec3 h = cross(r.dir, edge2);
	const float a = dot(edge1, h);
	if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
	const float f = 1 / a;
	const vec3 s = r.pos - v0;
	const float u = f * dot(s, h);
	if (u < 0 || u > 1) return;
	const vec3 q = cross(s, edge1);
	const float v = f * dot(r.dir, q);
	if (v < 0 || u + v > 1) return;
	const float t = f * dot(edge2, q);
	if (t > 0.0001f) {
		t_out = t;
		bary_out = vec3(u, v, 1 - u - v);
	}
}
bool IntersectRayWorld(Ray r, float tmin, float tmax, RayHit* out, const BVH& bvh, const Level* level)
{
	const BVHNode* stack[64];
	stack[0] = &bvh.nodes[0];
	int stack_count = 1;
	const BVHNode* node = nullptr;
	float t = INFINITY;
	vec3 bary = vec3(0);
	int triindex = -1;
	const Level::CollisionData& cd = level->collision_data;
	while (stack_count > 0)
	{
		node = stack[--stack_count];

		if (node->count != BVH_BRANCH)
		{
			int index_count = node->count;
			int index_start = node->left_node;
			for (int i = 0; i < index_count; i++) {
				vec3 bary_temp = vec3(0);
				vec3 N_temp;
				float t_temp = -1;

				int mesh_element_index = bvh.indicies[index_start + i];

				ASSERT(mesh_element_index < cd.collision_tris.size());
				const Level::CollisionTri& tri = cd.collision_tris[mesh_element_index];

				IntersectTriRay2(r, cd.vertex_list[tri.indicies[0]],
					cd.vertex_list[tri.indicies[1]],
					cd.vertex_list[tri.indicies[2]], t_temp, bary_temp);
				bool res = t_temp > 0;


				if (!res || t_temp > tmax || t_temp < tmin)
					continue;
				tmax = t_temp;
				t = t_temp;
				triindex = mesh_element_index;
			}
			continue;
		}

		float left_dist, right_dist;
		bool left_aabb = bvh.nodes[node->left_node].aabb.intersect(r, left_dist);
		bool right_aabb = bvh.nodes[node->left_node + 1].aabb.intersect(r, right_dist);
		left_aabb = left_aabb && left_dist < t;
		right_aabb = right_aabb && right_dist < t;

		if (left_aabb && right_aabb) {
			if (left_dist < right_dist) {
				stack[stack_count++] = &bvh.nodes[node->left_node + 1];
				stack[stack_count++] = &bvh.nodes[node->left_node];
			}
			else {
				stack[stack_count++] = &bvh.nodes[node->left_node];
				stack[stack_count++] = &bvh.nodes[node->left_node + 1];
			}
		}
		else if (left_aabb) {
			stack[stack_count++] = &bvh.nodes[node->left_node];
		}
		else if (right_aabb) {
			stack[stack_count++] = &bvh.nodes[node->left_node + 1];
		}
	}

	if (!std::isfinite(t)) {
		return false;
	}

	out->pos = r.at(t);
	const auto& tri = cd.collision_tris.at(triindex);
	out->normal = tri.face_normal;
	out->part_id = 0;
	out->surf_type = tri.surf_type;
	out->dist = t;
	out->id = 0;

	return true;
}

void TraceRayAgainstLevel(const Level* level, Ray r, RayHit* out, bool closest)
{
	out->dist = -1.f;
	IntersectRayWorld(r, 0.f, 1000.f, out, level->static_geo_bvh, level);
}

void Capsule::GetSphereCenters(vec3& a, vec3& b) const
{
	vec3 cap_normal = normalize(tip - base);
	vec3 line_end_ofs = cap_normal * radius;
	a = base + line_end_ofs;
	b = tip - line_end_ofs;
}
