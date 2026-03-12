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
class LuaMiscFuncs : public ClassBase {
public:
	CLASS_BODY(LuaMiscFuncs, scriptable);
	static LuaMiscFuncs* inst;
	REF virtual Entity* create_ragdoll() {
		return nullptr;
	}
};

class CameraPathFollower {
public:
	CameraPathFollower(std::vector<SpawnerComponent*> components);
	static glm::vec3 catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
	void update();

	struct Point {
		glm::vec3 p;
		glm::quat q;
	};
	std::vector<Point> points;
	double time_per_point = 6.0;
	double time_start = 0.0;
};

class EngineTestcase {
public:
	virtual ~EngineTestcase() {}
	// always called
	virtual bool update(bool focused) { return false; }	// return false if skip to next focused
	// called for 'integration testing' can sleep ticks or time, then confirm output
	virtual void integration_tick(IntegrationTester& t) {}

	string name{};
};

class EngineTesterApp : public Application {
public:
	CLASS_BODY(EngineTesterApp, scriptable);

	const glm::ivec2 TEST_WINDOW_SIZE = glm::ivec2{ 800,600 };

	EngineTesterApp();

	// funs for lua to override for testing
	REF virtual Entity* make_object_test() {
		return nullptr;
	}
	REF virtual bool make_object_test_return_exists() {
		return false;
	}
	REF virtual int count_in_vector(std::vector<int> nums) {
		return 0;
	}
	
	static std::vector<SpawnerComponent*> find_all_with_name_ordered(string name, string class_);

	static std::vector<SpawnerComponent*> find_all_in_class(string name);
	void start();
	void update();


	uptr<IntegrationTester> tester;
};