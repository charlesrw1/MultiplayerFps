#ifndef PHYSICS_H
#define PHYSICS_H
#include "glm/glm.hpp"
#include "MathLib.h"
#include <vector>
#include "Level.h"

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

struct TriangleShape
{
	int indicies[3];
	glm::vec3 face_normal;
	float plane_offset = 0.f;
	short surf_flags = 0;
	short surf_type = 0;
};


class BVH;
struct MeshShape
{
	const std::vector<glm::vec3>* verticies;
	const std::vector<Level::CollisionTri>* tris;
	const BVH* structure;
};

struct CharacterShape
{
	glm::vec3 org;
	float height;
	float radius;

	const Animator* a;
	const Model* m;

	Bounds ToBounds();
};
struct SphereShape
{
	SphereShape() {}
	SphereShape(glm::vec3 origin, float r) 
		: origin(origin), radius(r) {}

	glm::vec3 origin;
	float radius;
	Bounds ToBounds();
};
struct BoxShape
{
	glm::vec3 min;
	glm::vec3 max;
	glm::mat3 rot;
	Bounds ToBounds();
};

struct PhysicsObject
{
	PhysicsObject() {

	}
	enum Type {
		Sphere,
		Character,
		Box,
		Mesh,
	} shape;
	union {
		CharacterShape character;
		SphereShape sphere;
		BoxShape box;
		MeshShape mesh;
	};
	int userindex = -1;
	bool player = false;
	bool solid = true;
	bool is_level = false;
};


enum PhysFilterFlags
{
	Pf_World= 1,
	Pf_Players = 2,
	Pf_Nonplayers = 4,

	Pf_All = Pf_World | Pf_Players | Pf_Nonplayers
};

class Level;
class PhysicsWorld
{
public:

	void AddLevel(const Level* l);
	void AddObj(PhysicsObject obj);
	void ClearObjs();

	void TraceRay(Ray r, RayHit* out, int ignore_index, int filter_flags);
	void TraceCharacter(CharacterShape shape, GeomContact* c, int ignore_index, int filter_flags);	// capsule shaped character
	void TraceSphere(SphereShape shape, GeomContact* c, int ignore_index, int filter_flags);
	void GetObjectsInBox(int* indicies, int buffer_len, Bounds box, int filter_flags);
	void GetObjectsInRadius(int* indicies, int buffer_len, glm::vec3 org, float r, int filter_flags);

	PhysicsObject& GetObj(int index);
private:
	bool FilterObj(PhysicsObject* po, int ignore_index, int filter_flags);

	std::vector<PhysicsObject> objs;
};

class Level;
void DrawCollisionWorld(const Level* lvl);
Bounds CapsuleToAABB(const Capsule& cap);

// Called by the level loader to init the bvh
void InitStaticGeoBvh(Level* input);


#endif // !PHYSICS_H
