#include "Physics.h"
#include <vector>
#include "GlmInclude.h"
#include "Framework/MathLib.h"
#include "Level.h"
#include "Framework/MeshBuilder.h"
#include "glad/glad.h"
#include "Framework/BVH.h"
#include "Types.h"
#include "Render/Model.h"


void BoxVsBox(Bounds b1, Bounds b2, GeomContact* out);

static MeshBuilder world_collision;
void DrawCollisionWorld(const Level* lvl)
{
#if 0
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
#endif
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
				int index = bvh.indicies[index_start + i];
				bool should_exit = do_intersect(index);
				if (should_exit)
					return;
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
}

bool SphereVsTriangle(glm::vec3 corners[], glm::vec3 facen, float plantofs, 
	glm::vec3 pos, float radius, GeomContact* out, bool doublesided)
{
	float dist = dot(pos, facen) + plantofs;
	if (dist < 0 && !doublesided)
		return false;
	if (dist > radius)
		return false;

	vec3 pointontri = pos - facen * dist;
	vec3 c0 = cross(pointontri - corners[0], corners[1] - corners[0]);
	vec3 c1 = cross(pointontri - corners[1], corners[2] - corners[1]);
	vec3 c2 = cross(pointontri - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, facen) <= 0 && dot(c1, facen) <= 0 && dot(c2, facen) <= 0;

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
	out->surf_normal = facen;

	return true;
}

vec3 CapsuleGetReferencePoint(glm::vec3 corners[], glm::vec3 facen,
	const Capsule& cap, vec3 line_plane_intersect)
{

	// Determine if intersection point is inside triangle
	vec3 c0 = cross(line_plane_intersect - corners[0], corners[1] - corners[0]);
	vec3 c1 = cross(line_plane_intersect - corners[1], corners[2] - corners[1]);
	vec3 c2 = cross(line_plane_intersect - corners[2], corners[0] - corners[2]);
	bool inside = dot(c0, facen) <= 0 && dot(c1, facen) <= 0 && dot(c2, facen) <= 0;

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

bool CapsuleVsTriangle(glm::vec3 corners[], glm::vec3 facen, float planeofs, 
	const Capsule& cap, GeomContact* out)
{
	vec3 cap_normal = normalize(cap.tip - cap.base);
	vec3 line_end_ofs = cap_normal * cap.radius;
	vec3 A = cap.base + line_end_ofs;
	vec3 B = cap.tip - line_end_ofs;

	//glm::vec3 corners[3];
	//for (int i = 0; i < 3; i++)
	//	corners[i]=(*mesh->verticies)[tri.indicies[i]];

	float denom = dot(facen, cap_normal);

	// Intersect capsule normal (line segment from base to tip) with triangle's plane
	if (abs(denom) > 0.15) {	// such a high epsilon to deal with weird issues
		float planedist = dot(facen, cap.base) + planeofs;
		float time = -planedist / denom;
		vec3 line_plane_intersect = cap.base + cap_normal * time;

		vec3 reference_point = CapsuleGetReferencePoint(corners,facen, cap, line_plane_intersect);

		vec3 sphere_center = closest_point_on_line(A, B, reference_point);

		// Finish with a sphere/triangle test
		return SphereVsTriangle(corners,facen,planeofs, sphere_center, cap.radius, out, false);
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
			if (SphereVsTriangle(corners, facen, planeofs, sphere_center, cap.radius, out, false))
				return true;
		}
		{
			vec3 sphere_center = closest_point_on_line(A, B, ref_low);
			return (SphereVsTriangle(corners, facen, planeofs, sphere_center, cap.radius, out, false));
		}

	}
}

Bounds CapsuleToAABB(vec3 org,float height, float radius)
{
	vec3 base = org;
	vec3 tip = org + vec3(0, height, 0);

	vec3 normal = normalize(tip - base);
	vec3 centertip = tip - normal * radius;
	vec3 centerbase = base + normal * radius;
	return bounds_union(Bounds(centertip - vec3(radius), centertip + vec3(radius)),
		Bounds(centerbase - vec3(radius), centerbase + vec3(radius)));
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

void Capsule::GetSphereCenters(vec3& a, vec3& b) const
{
	vec3 cap_normal = normalize(tip - base);
	vec3 line_end_ofs = cap_normal * radius;
	a = base + line_end_ofs;
	b = tip - line_end_ofs;
}

GeomContact shape_vs_tri_mesh(Trace_Shape shape, MeshShape* mesh)
{
	float best_len_total = INFINITY;
	Bounds bounds;
	GeomContact temp;
	GeomContact final;
	final.found = false;

	if (shape.sphere) {

		auto sphere_intersect_functor = [&](int index)->bool {
			ASSERT(index < (mesh->verticies->size()));
			const Physics_Triangle& tri = (*mesh->tris)[index];
			glm::vec3 corners[3];
			for (int i = 0; i < 3; i++)
				corners[i] = (*mesh->verticies)[tri.indicies[i]];

			bool res = SphereVsTriangle(corners, tri.face_normal, tri.plane_offset, shape.pos, shape.radius, &temp, false);
			if (res && temp.intersect_len < best_len_total) {
				best_len_total = temp.intersect_len;
				final = temp;
			}
			return false;
		};
		Bounds sphere(vec3(-shape.radius)+shape.pos, vec3(shape.radius)+shape.pos);
		IntersectWorld(sphere_intersect_functor, *mesh->structure, sphere);

	}
	else {
		Capsule cap;
		cap.base = shape.pos;
		cap.radius = shape.radius;
		cap.tip = cap.base + vec3(0, shape.height, 0);

		Bounds b2 = CapsuleToAABB(shape.pos, shape.height, shape.radius);
		b2.bmin -= vec3(0.05);
		b2.bmax += vec3(0.05);

		bool found_ground = false;
		GeomContact temp;

		auto capsule_intersect_functor = [&](int index) -> bool {
			ASSERT(index < mesh->tris->size());
			const Physics_Triangle& tri = (*mesh->tris)[index];

			glm::vec3 corners[3];
			for (int i = 0; i < 3; i++)
				corners[i] = (*mesh->verticies)[tri.indicies[i]];

			bool res = CapsuleVsTriangle(corners, tri.face_normal, tri.plane_offset, cap, &temp);
			if (res && temp.intersect_len < best_len_total) {
				best_len_total = temp.intersect_len;
				final = temp;
			}
			return false;
		};
		IntersectWorld(capsule_intersect_functor, *mesh->structure, b2);

		final.touched_ground = found_ground;
	}


	return final;
}

Trace_Shape::Trace_Shape(vec3 org, float radius)
{
	sphere = true;
	this->radius = radius;
	this->pos = org;
}
Trace_Shape::Trace_Shape()
{

}
Trace_Shape::Trace_Shape(vec3 org, float radius, float height)
{
	sphere = false;
	this->height = height;
	this->radius = radius;
	this->pos = org;
}

Bounds Trace_Shape::to_bounds()
{
	if (sphere) {
		return Bounds(vec3(-radius + pos), vec3(radius + pos));
	}
	return CapsuleToAABB(pos, height, radius);
}
Bounds PhysicsObject::to_bounds()
{
	return Bounds(min_or_origin, max);
}


GeomContact shape_vs_bounds(Trace_Shape& shape, PhysicsObject& obj)
{
	GeomContact gc;
	BoxVsBox(shape.to_bounds(), obj.to_bounds(),&gc);
	return gc;
}

bool IntersectRayMesh(Ray r, float tmin, float tmax, RayHit* out, MeshShape* s)
{
	const BVH& bvh = *s->structure;
	const BVHNode* stack[64];
	stack[0] = &bvh.nodes[0];
	int stack_count = 1;
	const BVHNode* node = nullptr;
	float t = INFINITY;
	vec3 bary = vec3(0);
	int triindex = -1;
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

				ASSERT(mesh_element_index <(s->tris->size()));
				const Physics_Triangle& tri =(*s->tris)[mesh_element_index];

				IntersectTriRay2(r, (*s->verticies)[tri.indicies[0]],
					(*s->verticies)[tri.indicies[1]],
					(*s->verticies)[tri.indicies[2]], t_temp, bary_temp);
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
	const auto& tri = s->tris->at(triindex);
	out->normal = tri.face_normal;
	out->part_id = 0;
	out->surf_type = tri.surf_type;
	out->dist = t;
	out->ent_id = 0;

	return true;
}

void ray_vs_mesh(Ray r, RayHit* rh, PhysicsObject* mesh)
{
	IntersectRayMesh(r, 0, INFINITY, rh, &mesh->mesh);
	if (rh->dist > 0) {
		if (mesh->is_level) rh->hit_world = true;
		rh->ent_id = mesh->userindex;
	}
}


void BoxVsBox(Bounds b1, Bounds b2, GeomContact* out)
{
	out->found = false;
	float x1 = b1.bmax.x - b2.bmin.x;
	float x2 = b2.bmax.x - b1.bmin.x;
	float y1 = b1.bmax.y - b2.bmin.y;
	float y2 = b2.bmax.y - b1.bmin.y;
	float z1 = b1.bmax.z - b2.bmin.z;
	float z2 = b2.bmax.z - b1.bmin.z;

	bool boxoverlap = x1 >= 0 && x2 >= 0 
		&& y1 >= 0 && y2 >= 0 
		&& z1 >= 0 && z2 >= 0;
	if (!boxoverlap)
		return;

	out->found = true;
	// if it overlaps, the best side to push out is the smallest overlap one
	float smallest = glm::min(glm::min(glm::min(glm::min(glm::min(x1, x2), y1), y2), z1), z2);
	if (x1 == smallest)
		out->penetration_normal = vec3(1, 0, 0);
	else if (x2 == smallest)
		out->penetration_normal = vec3(-1, 0, 0);
	else if (y1 == smallest)
		out->penetration_normal = vec3(0, 1, 0);
	else if (y2 == smallest)
		out->penetration_normal = vec3(0, -1, 0);
	else if (z1 == smallest)
		out->penetration_normal = vec3(0, 0, 1);
	else if (z2 == smallest)
		out->penetration_normal = vec3(0, 0, -1);
	out->penetration_normal *= -1;
	out->penetration_depth = smallest;
	out->surf_normal = -out->penetration_normal;
	out->intersect_len = dot((b1.bmax - b1.bmin)*0.5f * glm::abs(out->penetration_normal),vec3(1))-out->penetration_depth;

}

void ray_vs_shape(Ray r, PhysicsObject& obj, RayHit* rh)
{
	// trace against hitboxes
	if (0) {
		sys_print("hitbox trace");
	}
	else {
		Bounds b = obj.to_bounds();

		float t_out = 0.0;
		bool intersects = b.intersect(r, t_out);

		if (intersects) {
			rh->dist = t_out;
			rh->ent_id = obj.userindex;
			rh->hit_world = false;
			rh->pos = r.at(t_out);
		}
	}
}


void PhysicsWorld::AddObj(PhysicsObject obj)
{
	objs.push_back(obj);
}
void PhysicsWorld::ClearObjs()
{
	objs.clear();
}

GeomContact PhysicsWorld::trace_shape(Trace_Shape shape, int ig, int filt)
{
	GeomContact gc;
	gc.found = false;
	gc.intersect_len = INFINITY;

	return gc;

	for (int i = 0; i < objs.size(); i++)
	{
		if (FilterObj(&objs[i], ig, filt))
			continue;

		GeomContact contact;
		if (objs[i].is_mesh) {
			Trace_Shape shape2 = shape;
			if (!objs[i].is_level) {
				shape2.pos = objs[i].inverse_transform * vec4(shape2.pos, 1.0);
			}
			contact = shape_vs_tri_mesh(shape2, &objs[i].mesh);
			if (contact.found && !objs[i].is_level) {
				contact.penetration_normal = mat3(objs[i].transform) * vec3(contact.penetration_normal);
				contact.surf_normal = mat3(objs[i].transform) * vec3(contact.surf_normal);
				contact.intersect_point = objs[i].transform * vec4(contact.intersect_point,1.0);
			}
		}
		else
			contact = shape_vs_bounds(shape, objs[i]);

		if (contact.found && contact.intersect_len < gc.intersect_len)
			gc = contact;

	}

	return gc;
}

RayHit PhysicsWorld::trace_ray(Ray r, int ignore_index, int filter_flags)
{
	RayHit final;
	final.dist = -1;

	return final;

	for (int i = 0; i < objs.size(); i++)
	{
		auto& obj = objs[i];

		if (FilterObj(&obj, ignore_index, filter_flags))
			continue;

		RayHit hit;
		if (obj.is_mesh) {
			Ray r2 = r;
			if (!objs[i].is_level) {
				r2.pos = objs[i].inverse_transform * vec4(r2.pos, 1.0);
				r2.dir = mat3(objs[i].inverse_transform) * r2.dir;
			}
			ray_vs_mesh(r2, &hit, &obj);
			if (hit.dist > 0 && !objs[i].is_level) {
				hit.pos = objs[i].transform * vec4(hit.pos,1.0);
				hit.normal = mat3(objs[i].transform) * hit.normal;
			}
		}
		else
			ray_vs_shape(r, obj, &hit);

		if (hit.dist > 0.f && (final.dist < 0 || hit.dist < final.dist))
			final = hit;
	}

	return final;
}

bool PhysicsWorld::FilterObj(PhysicsObject* o, int ig_ent, int filter_flags)
{
	if (!o->is_level && o->userindex == ig_ent)
		return true;
	
	if (o->is_level && !(filter_flags & PF_WORLD))
		return true;

	return false;
}
