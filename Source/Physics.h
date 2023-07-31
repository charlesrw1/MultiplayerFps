#ifndef PHYSICS_H
#define PHYSICS_H
#include "glm/glm.hpp"
#include "MathLib.h"
struct ColliderCastResult
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

class Actor;
struct RayCastResult
{
	glm::vec3 startpos;
	glm::vec3 endpos;
	glm::vec3 dir;
	float t;	// -1 hit sky

	// plane of surface
	glm::vec3 hitnormal;
	float hitd;

	Actor* hitactor = nullptr;	// null if hit world
	int model_hitbox = -1;	// if applicable
};

struct Capsule
{
	float radius = 1.0f;;
	glm::vec3 base = glm::vec3(0.0,2.0,0.0);
	glm::vec3 tip=glm::vec3(0.0);

	void GetSphereCenters(glm::vec3& a, glm::vec3& b) const;
};

class Level;
void DrawCollisionWorld(const Level* lvl);
Bounds CapsuleToAABB(const Capsule& cap);
void TraceCapsule(const Level* lvl, glm::vec3 org, const Capsule& capsule, ColliderCastResult* out, bool closest);
void TraceSphere(const Level* lvl, glm::vec3 org, float radius, ColliderCastResult* out, bool closest, bool double_sided);

// Called by the level loader to init the bvh
void InitStaticGeoBvh(Level* input);


#endif // !PHYSICS_H
