#pragma once
#include <SDL2/SDL.h>
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
#include "Framework/Config.h"
#include "Game/Components/MeshComponent.h"
#include "Animation/Runtime/Animation.h"
#include "IntegrationTest.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Game/Components/RagdollComponent.h"
#include "Render/DynamicMaterialPtr.h"
#include "Render/MaterialPublic.h"
class LuaMiscFuncs : public ClassBase
{
public:
	CLASS_BODY(LuaMiscFuncs, scriptable);
	static LuaMiscFuncs* inst;
	REF virtual Entity* create_ragdoll() { return nullptr; }
};

class CameraPathFollower
{
public:
	CameraPathFollower(std::vector<SpawnerComponent*> components);
	static glm::vec3 catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
	void update();

	struct Point
	{
		glm::vec3 p;
		glm::quat q;
	};
	std::vector<Point> points;
	double time_per_point = 6.0;
	double time_start = 0.0;
};
template <typename T> inline T negative_modulo(T x, T mod_) {
	return ((x % mod_) + mod_ % mod_);
}
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/MathLib.h"
#include "Game/Entities/Player.h"
struct CamPathPoints
{
	STRUCT_BODY();

	REF float time = 1.0;
	REF std::vector<lTransform> points;
};
class CamPathFollowerLua : public ClassBase
{
public:
	CLASS_BODY(CamPathFollowerLua);

	REF void clear_all() { paths.clear(); }
	REF void add(CamPathPoints points) { paths.push_back(points); }
	REF void goto_next() {
		cur_time = 0.0;
		cur_idx = (cur_idx + 1) % paths.size();
	}
	REF void goto_prev() {
		cur_time = 0.0;
		cur_idx -= 1;
		if (cur_idx < 0)
			cur_idx += paths.size();
	}
	REF void update() {
		if (paths.empty())
			return;
		if (cur_idx < 0 || cur_idx >= paths.size())
			cur_idx = 0;
		auto& path = paths.at(cur_idx);
		auto scene_cam = CameraComponent::get_scene_camera();
		if (!scene_cam)
			return;
		if (path.points.size() == 0) {
			goto_next();
			return;
		}
		if (path.points.size() == 1) {
			auto& p0 = path.points.at(0);
			scene_cam->get_owner()->set_ws_position_rotation(p0.pos, p0.rot);
		} else {
			float time_per_point = path.time / (path.points.size() - 1);
			int first = std::floor(cur_time / time_per_point);
			float frac = fmod(cur_time, time_per_point) / time_per_point;
			lTransform& p0 = path.points.at(first);
			lTransform& p1 = path.points.at(first + 1);
			scene_cam->get_owner()->set_ws_position_rotation(glm::mix(glm::vec3(p0.pos), glm::vec3(p1.pos), frac),
															 glm::slerp(glm::quat(p0.rot), glm::quat(p1.rot), frac));
			GameplayStatic::debug_text(string_format("%i %.3f", first, frac));
		}
		float dt = GameplayStatic::get_dt();
		cur_time += dt;
		if (cur_time >= path.time) {
			goto_next();
		}
	}
	float cur_time = 0.0;
	int cur_idx = -1;
	std::vector<CamPathPoints> paths;
};

class EngineTestcase
{
public:
	virtual ~EngineTestcase() {}
	// always called
	virtual bool update(bool focused) { return false; } // return false if skip to next focused
	// called for 'integration testing' can sleep ticks or time, then confirm output
	virtual void integration_tick(IntegrationTester& t) {}

	string name{};
};

class EngineTesterApp : public Application
{
public:
	CLASS_BODY(EngineTesterApp, scriptable);

	const glm::ivec2 TEST_WINDOW_SIZE = glm::ivec2{800, 600};

	EngineTesterApp();
	REF virtual void lua_start() {}

	// funs for lua to override for testing
	REF virtual Entity* make_object_test() { return nullptr; }
	REF virtual bool make_object_test_return_exists() { return false; }
	REF virtual int count_in_vector(std::vector<int> nums) { return 0; }

	static std::vector<SpawnerComponent*> find_all_with_name_ordered(string name, string class_);

	static std::vector<SpawnerComponent*> find_all_in_class(string name);
	void start();
	void update();

	uptr<IntegrationTester> tester;
};