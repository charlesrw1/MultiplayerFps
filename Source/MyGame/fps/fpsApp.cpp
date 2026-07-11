#include "fpsApp.h"
#include "fpsObjects.h"
#include "../../Game/GameplayStatic.h"
#include "../../Game/Entities/CharacterController.h"
#include "../../Game/Components/CameraComponent.h"
#include "Input/InputSystem.h"

extern void Quit();
fpsApp* fpsApp::inst = nullptr;

void fpsApp::start() {
	ASSERT(!inst);
	inst = this;

	game = std::make_unique<fpsGameMgr>();
	lua.reset(ClassBase::create_class<fpsLuaBridge>("fpsLuaBridgeImpl"));
	if (!lua)
		sys_print(Warning, "no fpsLuaBridgeImpl");
	else
		lua->init();
	game->start_level("physics_test_world.tmap");

	// a comment
}

void fpsApp::update() {
	if (Input::is_alt_down() && Input::was_key_pressed(SDL_SCANCODE_F4))
		Quit();
	game->update();
	if (lua)
		lua->update();

	// test comment
}

void fpsApp::stop() {
	game->stop();
	inst = nullptr;
}

void fpsApp::on_imgui() {
	game->debug_camera.on_imgui();
	lua->imgui_tick();
}

void fpsGameMgr::update() {
	auto* playerptr = player->get_component<fpsPlayer>();
	playerptr->manualtick();

	// pass game camera entity to debug camera for switching
	debug_camera.update(playerptr->camera);
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

	debug_camera.init();
}

void fpsGameMgr::stop() {
	debug_camera.shutdown();
}
