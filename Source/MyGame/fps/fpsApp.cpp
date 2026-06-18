#include "fpsApp.h"
#include "fpsObjects.h"
#include "../../Game/GameplayStatic.h"
#include "../../Game/Entities/CharacterController.h"
#include "../../Game/Components/CameraComponent.h"
fpsApp* fpsApp::inst = nullptr;

void fpsApp::start() {
	ASSERT(!inst);
	inst = this;

	game = std::make_unique<fpsGameMgr>();
	game->start_level("physics_test_world.tmap");
}

void fpsApp::update() {
	game->update();
}
void fpsGameMgr::update() {
	auto* playerptr = player->get_component<fpsPlayer>();
	playerptr->manualtick();
	playerptr->camera->get_component<CameraComponent>()->set_is_enabled(true);
}

void fpsGameMgr::start_level(const std::string& name) {
	bool success = GameplayStatic::change_level(name);
	ASSERT(success);

	// load in player
	auto spawn_points = GameplayStatic::find_components(&fpsSpawnPoint::StaticType);
	ASSERT(!spawn_points.empty());

	fpsSpawnPoint* point = spawn_points.at(0)->cast_to<fpsSpawnPoint>();

	auto create_player = [&]() -> Entity* {
		Entity* pe = GameplayStatic::spawn_entity();
		pe->set_ws_transform(point->get_ws_transform());
		auto* cc = pe->create_component<CapsuleComponent>();
		auto* cmc = pe->create_component<CharacterMovementComponent>();
		cmc->set_physics_body(cc);
		pe->create_component<fpsPlayer>();

		return pe;
	};
	auto* player = create_player();
	this->player = player;
}
