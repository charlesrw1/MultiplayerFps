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

// only does xz plane collisions
void CylinderCylinderIntersect(float r1, glm::vec3 o1, float h1, float r2, glm::vec3 o2, float h2, GeomContact* out)
{
	out->found = false;
	bool y_overlap = o1.y <= o2.y + h2 && o1.y + h1 >= o2.y;
	float yoverlap_amt = glm::min(glm::abs(o2.y + h2 - o1.y), glm::abs(o1.y + h1 - o2.y));
	glm::vec3 yintersectvect = glm::vec3(0, o1.y - (o2.y + h2), 0);
	glm::vec3 yintersectvec2 = glm::vec3(0, o2.y - (o1.y + h1), 0);
	if (glm::abs(yintersectvec2.y) < glm::abs(yintersectvect.y))
		yintersectvect = yintersectvec2;

	glm::vec3 xzintersectvec = glm::vec3(o1.x - o2.x, 0, o1.z - o2.z);
	float xz_dist = glm::length(xzintersectvec);
	bool xzoverlap = xz_dist <= r1 + r2;

	if (!y_overlap || !xzoverlap)
		return;

	out->found = true;
	out->penetration_depth = r1+r2-xz_dist;
	out->penetration_normal = xzintersectvec/xz_dist;
	out->intersect_len = r1 - out->penetration_depth;
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

Bounds CharacterShape::ToBounds()
{
	Bounds b;
	b.bmin = org - glm::vec3(radius, 0, radius);
	b.bmax = org + glm::vec3(radius, height, radius);
	return b;
}
Bounds BoxShape::ToBounds()
{
	Bounds b;
	b.bmin = min;
	b.bmax = max;
	return b;
}
Bounds SphereShape::ToBounds()
{
	Bounds b;
	b.bmin = origin - glm::vec3(radius);
	b.bmax = origin + glm::vec3(radius);
	return b;
}

void SphereVsTriMesh(SphereShape* s, MeshShape* mesh, GeomContact* gc)
{
	float best_len_total = INFINITY;

	GeomContact temp;
	auto sphere_intersect_functor = [&](int index)->bool {
		ASSERT(index < (mesh->verticies->size()));
		const Level::CollisionTri& tri = (*mesh->tris)[index];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = (*mesh->verticies)[tri.indicies[i]];
		bool res = SphereVsTriangle(corners, tri.face_normal,tri.plane_offset, s->origin, s->radius, &temp, true);
		if (res && temp.intersect_len < best_len_total) {
			best_len_total = temp.intersect_len;
			*gc = temp;
			//if (!closest)
			//	return true;
		}
		return false;
	};
	Bounds sphere(vec3(-s->radius + s->origin), vec3(s->radius + s->origin));
	IntersectWorld(sphere_intersect_functor, *mesh->structure, sphere);
}

void SphereVsBox()
{

}

void SphereVsCharShape(SphereShape* s, CharacterShape* cs, GeomContact* gc)
{
	SphereVsBox();
}


void CharShapeVsTriMesh(CharacterShape* cs, MeshShape* mesh, GeomContact* gc)
{
	Capsule cap;
	cap.base = cs->org;
	cap.radius = cs->radius;
	cap.tip = cap.base + vec3(0, cs->height, 0);

	Bounds b2 = CapsuleToAABB(cap);
	b2.bmin -= vec3(0.05);
	b2.bmax += vec3(0.05);

	float best_len_total = INFINITY;

	bool found_ground = false;
	GeomContact temp;

	auto capsule_intersect_functor = [&](int index) -> bool {
		ASSERT(index < mesh->tris->size());
		const Level::CollisionTri& tri = (*mesh->tris)[index];

		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = (*mesh->verticies)[tri.indicies[i]];

		bool res = CapsuleVsTriangle(corners, tri.face_normal, tri.plane_offset,cap, &temp);
		if (res && temp.intersect_len < best_len_total) {
			best_len_total = temp.intersect_len;
			*gc = temp;
			//if (!closest)
			//	return true;
		}
		return false;
	};
	IntersectWorld(capsule_intersect_functor, *mesh->structure, b2);

	gc->touched_ground = found_ground;
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
				const Level::CollisionTri& tri =(*s->tris)[mesh_element_index];

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

void RayVsTriMesh(Ray r, RayHit* rh, PhysicsObject* mesh)
{
	IntersectRayMesh(r, 0, INFINITY, rh, &mesh->mesh);
	if (rh->dist > 0 && mesh->is_level)
		rh->hit_world = true;
}

void RayVsBox()
{

}

void BoxVsBox(Bounds b1, Bounds b2, GeomContact* out)
{
	//bool xoverlap = bmin.x <= other.bmax.x && bmax.x >= other.bmin.x;
	//bool yoverlap = bmin.y <= other.bmax.y && bmax.y >= other.bmin.y;
	//bool zoverlap = bmin.z <= other.bmax.z && bmax.z >= other.bmin.z;
}

void RayVsCharShape(PhysicsObject* character, Ray r, RayHit* rh)
{
	CharacterShape* cs = &character->character;
	Bounds b = cs->ToBounds();
	float t_out=0.0;
	bool intersects = b.intersect(r, t_out);

	if (intersects) {
		rh->dist = t_out;
		rh->ent_id = character->userindex;
		rh->hit_world = false;
		rh->pos = r.at(t_out);
	}


}

void CharShapeVsCharShape(CharacterShape* cs, CharacterShape* cs2, GeomContact* out)
{
	//BoxVsBox(cs->ToBounds(), cs2->ToBounds(), out);
	CylinderCylinderIntersect(cs->radius, cs->org, cs->height, cs2->radius, cs2->org, cs2->height, out);
}

void CharShapeVsBox(CharacterShape* cs, BoxShape* b, GeomContact* out)
{
	BoxVsBox(cs->ToBounds(), b->ToBounds(), out);
}

void PhysicsWorld::AddObj(PhysicsObject obj)
{
	objs.push_back(obj);
}
void PhysicsWorld::ClearObjs()
{
	objs.clear();
}
void PhysicsWorld::AddLevel(const Level* l)
{
	PhysicsObject obj;
	obj.shape = PhysicsObject::Mesh;
	obj.is_level = true;
	obj.solid = true;

	obj.mesh.structure = &l->static_geo_bvh;
	obj.mesh.verticies = &l->collision_data.vertex_list;
	obj.mesh.tris = &l->collision_data.collision_tris;

	objs.push_back(obj);
}


void PhysicsWorld::TraceCharacter(CharacterShape c, GeomContact* gc, int ignore_index, int filter_flags)
{
	gc->found = false;
	gc->intersect_len = INFINITY;

	for (int i = 0; i < objs.size(); i++)
	{
		GeomContact contact;

		if (FilterObj(&objs[i], ignore_index, filter_flags))
			continue;

		switch (objs[i].shape)
		{
		//case PhysicsObject::Box:
		//	CharShapeVsBox(&c, &objs[i].box, &contact);
		//	break;
		case PhysicsObject::Character:
			CharShapeVsCharShape(&c, &objs[i].character, &contact);
			break;
		case PhysicsObject::Mesh:
			CharShapeVsTriMesh(&c, &objs[i].mesh, &contact);
			break;
		default:
			printf("unsupported TraceRay() type\n");
			break;
		}

		if (contact.found && contact.intersect_len < gc->intersect_len)
			*gc = contact;

	}
}

void PhysicsWorld::TraceSphere(SphereShape shape, GeomContact* gc, int ignore_index, int filter_flags)
{
	gc->found = false;
	gc->intersect_len = INFINITY;

	for (int i = 0; i < objs.size(); i++)
	{
		GeomContact contact;

		if (FilterObj(&objs[i], ignore_index, filter_flags))
			continue;

		switch (objs[i].shape)
		{
			//case PhysicsObject::Box:
			//	CharShapeVsBox(&c, &objs[i].box, &contact);
			//	break;
		case PhysicsObject::Character:
			SphereVsCharShape(&shape, &objs[i].character, &contact);
			break;
		case PhysicsObject::Mesh:
			SphereVsTriMesh(&shape, &objs[i].mesh, &contact);
			break;
		default:
			printf("unsupported TraceRay() type\n");
			break;
		}

		if (contact.found && contact.intersect_len < gc->intersect_len)
			*gc = contact;

	}
}

void PhysicsWorld::TraceRay(Ray r, RayHit* rh, int ignore_index, int filter_flags)
{
	for (int i = 0; i < objs.size(); i++)
	{
		auto& obj = objs[i];

		if (FilterObj(&obj, ignore_index, filter_flags))
			continue;

		switch (obj.shape)
		{
		//case PhysicsObject::Box:
		//	RayVsBox();
		//	break;
		case PhysicsObject::Character:
			RayVsCharShape(&obj, r, rh);
			break;
		case PhysicsObject::Mesh:
			RayVsTriMesh(r,rh, &obj);
			break;
		default:
			printf("unsupported TraceRay() type\n");
			break;
		}


	}
}

bool PhysicsWorld::FilterObj(PhysicsObject* o, int ig_ent, int filter_flags)
{
	if (o->userindex == ig_ent)
		return true;
	if (o->player && !(filter_flags & Pf_Players))
		return true;
	if (o->player && !(filter_flags & Pf_World))
		return true;
	return false;
}
