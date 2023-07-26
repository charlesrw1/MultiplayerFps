#include "Physics.h"
#include <vector>
#include "GlmInclude.h"
#include "MathLib.h"
#include "Level.h"
#include "MeshBuilder.h"
#include "glad/glad.h"

static MeshBuilder world_collision;
static bool has_built = false;
void DrawCollisionWorld()
{
	if (!has_built) {
		world_collision.Begin();
		Level::CollisionData& cd = TEMP_LEVEL->collision_data;
		for (int i = 0; i < cd.collision_tris.size(); i++) {
			Level::CollisionTri& ct = cd.collision_tris[i];
			world_collision.PushLine(cd.vertex_list[ct.indicies[0]], cd.vertex_list[ct.indicies[1]], COLOR_RED);
			world_collision.PushLine(cd.vertex_list[ct.indicies[1]], cd.vertex_list[ct.indicies[2]], COLOR_RED);
			world_collision.PushLine(cd.vertex_list[ct.indicies[2]], cd.vertex_list[ct.indicies[0]], COLOR_RED);

			glm::vec3 center = cd.vertex_list[ct.indicies[0]] + cd.vertex_list[ct.indicies[1]] + cd.vertex_list[ct.indicies[2]];
			center /= 3.f;
			world_collision.PushLine(center, center + ct.face_normal, COLOR_BLUE);
		}
		world_collision.End();
		has_built = true;
	}
	world_collision.Draw(GL_LINES);
}

bool SphereVsTriangle(const Level::CollisionData& cd, const Level::CollisionTri& tri, 
	glm::vec3 pos, float radius, ColliderCastResult* out)
{
	float dist = dot(pos, tri.face_normal) + tri.plane_offset;
	if (dist < 0)
		return false;
	if (dist > radius)
		return false;

	vec3 corners[3];
	for (int i = 0; i < 3; i++)
		corners[i] = cd.vertex_list[tri.indicies[i]];

	vec3 pointontri = pos - tri.face_normal * dist;
	vec3 c0 = -cross(pointontri - corners[0], corners[1] - corners[0]);
	vec3 c1 = -cross(pointontri - corners[1], corners[2] - corners[1]);
	vec3 c2 = -cross(pointontri - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, -tri.face_normal) <= 0 && dot(c1, -tri.face_normal) <= 0 && dot(c2, -tri.face_normal) <= 0;

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
	vec3 c0 = -cross(line_plane_intersect - corners[0], corners[1] - corners[0]);
	vec3 c1 = -cross(line_plane_intersect - corners[1], corners[2] - corners[1]);
	vec3 c2 = -cross(line_plane_intersect - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, -tri.face_normal) <= 0 && dot(c1, -tri.face_normal) <= 0 && dot(c2, -tri.face_normal) <= 0;

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

		vec3 p3 = closest_point_on_line(corners[2], corners[1], line_plane_intersect);
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
	const Capsule& cap, vec3 origin, ColliderCastResult* out)
{
	vec3 cap_normal = normalize(cap.tip - cap.base);
	vec3 line_end_ofs = cap_normal * cap.radius;
	vec3 A = cap.base + line_end_ofs;
	vec3 B = cap.tip - line_end_ofs;

	glm::vec3 corners[3];
	for (int i = 0; i < 3; i++)
		corners[i]=cd.vertex_list[tri.indicies[i]];

	// Set refrerence point to arbitrary point
	vec3 reference_point = corners[0];

	float denom = dot(tri.face_normal, cap_normal);

	// Intersect capsule normal (line segment from base to tip) with triangle's plane
	if (abs(denom) > 0.001) {
		float planedist = dot(tri.face_normal, cap.base) + tri.plane_offset;
		float time = -planedist / denom;
		vec3 line_plane_intersect = cap.base + cap_normal * time;

		reference_point = CapsuleGetReferencePoint(cd,tri, cap, line_plane_intersect);
	}


	vec3 sphere_center = closest_point_on_line(A, B, reference_point);

	// Finish with a sphere/triangle test
	return SphereVsTriangle(cd,tri, sphere_center, cap.radius, out);
}


void TraceCapsule(glm::vec3 pos, const Capsule& cap, ColliderCastResult* out)
{
	//if (!colctx.static_scene)
	//	return;
	//CollisionMeshData* world = colctx.static_scene;

	Level::CollisionData& cd = TEMP_LEVEL->collision_data;

	Capsule adjusted_cap = cap;
	adjusted_cap.base += pos;
	adjusted_cap.tip += pos;


	float best_len_total = INFINITY;

	bool found_ground = false;
	ColliderCastResult temp;
	for (int i = 0; i < cd.collision_tris.size(); i++) {
		const Level::CollisionTri& tri = cd.collision_tris[i];
		bool res = SphereVsTriangle(cd,tri, pos, cap.radius, &temp);
		if (res) {
			found_ground |= dot(temp.surf_normal, vec3(0, 1, 0)) > 0.7;
		}
		if (res && temp.intersect_len < best_len_total) {
			best_len_total = temp.intersect_len;
			*out = temp;
		}

		//bool res = CapsuleVsTriangle(cd, tri, adjusted_cap, vec3(0), &temp);
		//if (res) {
		//	found_ground |= dot(temp.surf_normal, vec3(0, 1, 0)) > 0.7;
		//}
		//if (res && temp.intersect_len < best_len_total) {
		//	best_len_total = temp.intersect_len;
		//	*out = temp;
		//}
	}
	out->touched_ground = found_ground;
}