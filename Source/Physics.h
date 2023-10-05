#ifndef PHYSICS_H
#define PHYSICS_H
#include "glm/glm.hpp"
#include "MathLib.h"

struct RayHit
{
	float dist=-1.f;
	glm::vec3 pos;
	glm::vec3 normal;
	int ent_id = 0;
	int part_id = 0;
	int surf_type = 0;
	bool hit_world = false;
};

struct GeomContact
{
	glm::vec3 penetration_normal;
	glm::vec3 intersect_point;
	float intersect_len;
	float penetration_depth;

	glm::vec3 surf_normal;
	float surf_d;
	short surf_type;
	short surf_flags;
	bool found = false;

	bool touched_ground = false;
};

struct Capsule
{
	float radius;
	glm::vec3 base;
	glm::vec3 tip;

	void GetSphereCenters(glm::vec3& a, glm::vec3& b) const;
};
struct Sphere
{
	float radius;
	glm::vec3 origin;
};

struct PhysContainer {
	enum Type {
		CapsuleType,
		SphereType
	} type;
	union {
		Capsule cap;
		Sphere sph;
	};
};

class Level;
void DrawCollisionWorld(const Level* lvl);
Bounds CapsuleToAABB(const Capsule& cap);

void CylinderCylinderIntersect(float r1, glm::vec3 o1, float h1, float r2, glm::vec3 o2, float h2, GeomContact* out);
void TraceAgainstLevel(const Level* lvl, GeomContact* out, PhysContainer obj, bool closest, bool double_sided);
void TraceCapsule(const Level* lvl, glm::vec3 org, const Capsule& capsule, GeomContact* out, bool closest);
void TraceSphere(const Level* lvl, glm::vec3 org, float radius, GeomContact* out, bool closest, bool double_sided);
void TraceRayAgainstLevel(const Level* lvl, Ray r, RayHit* out, bool closest);

// Called by the level loader to init the bvh
void InitStaticGeoBvh(Level* input);


#endif // !PHYSICS_H
