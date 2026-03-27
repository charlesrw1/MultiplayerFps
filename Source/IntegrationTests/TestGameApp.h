// Source/IntegrationTests/TestGameApp.h
#pragma once
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"

// Minimal Application subclass used by the integration-test game runner.
// Spawns a default camera so rendering has a valid view.
class TestGameApp : public Application
{
public:
	CLASS_BODY(TestGameApp, scriptable);

	void start() override {
		auto ent = eng->get_level()->spawn_entity();
		auto cam = ent->create_component<CameraComponent>();
		cam->set_is_enabled(true);
	}
};
