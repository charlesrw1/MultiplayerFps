#ifndef PHYSICS_H
#define PHYSICS_H
#include "glm/glm.hpp"
#include "MathLib.h"
#include <vector>
#include "Level.h"
#include "Model.h"

struct RayHit
{
	float dist=-1.f;
	glm::vec3 pos;
	glm::vec3 normal;
	int ent_id = -1;
	int part_id = -1;
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

class BVH;
struct MeshShape
{
	const std::vector<glm::vec3>* verticies;
	const std::vector<Physics_Triangle>* tris;
	const BVH* structure;
};

struct PhysicsObject
{
	bool is_mesh = false;
	glm::mat4 inverse_transform=glm::mat4(1.f);
	glm::mat4 transform=glm::mat4(1.f);
	MeshShape mesh;
	glm::vec3 max{};
	glm::vec3 min_or_origin{};
	const Animator* a = nullptr;
	const Model* m = nullptr;

	int userindex = -1;
	bool player = false;
	bool solid = true;
	bool is_level = false;

	Bounds to_bounds();
};

struct Trace_Shape
{
	Trace_Shape();
	Trace_Shape(glm::vec3 center, float radius);
	Trace_Shape(glm::vec3 org, float radius, float height);

	Bounds to_bounds();

	glm::vec3 pos{};
	float radius=1.f;
	float height=1.f;
	bool sphere=false;
};

enum Physics_Property
{
	PHY_DIRT,
	PHY_METAL,
	PHY_CONCRETE,
	PHY_ROCK,
	PHY_WATER,
	PHY_GLASS,
	PHY_ICE,
	PHY_SNOW,
};

enum Physics_Filter_Flags
{
	PF_WORLD = 1,
	PF_PLAYERS = 2,
	PF_NONPLAYERS = 4,

	PF_ALL = PF_WORLD | PF_PLAYERS | PF_NONPLAYERS
};

class Level;
class PhysicsWorld
{
public:
	void AddObj(PhysicsObject obj);
	void ClearObjs();

	RayHit trace_ray(Ray r, int ignore_index, int filter_flags);
	GeomContact trace_shape(Trace_Shape shape, int ignore_index, int filter_flags);


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
