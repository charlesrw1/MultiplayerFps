#include "fpsObjects.h"
#include "../../Game/GameplayStatic.h"
#include "../../Game/Entities/CharacterController.h"
#include "../../Game/Components/CameraComponent.h"
#include "fpsApp.h"

void fpsPlayer::start() {
	// create camera
	Entity* cament = GameplayStatic::spawn_entity();
	CameraComponent* camcomp = cament->create_component<CameraComponent>();
	this->camera = cament;
}

void fpsPlayer::manualtick() {
	camera->get_component<CameraComponent>()->get_owner()->set_ws_position(get_ws_position());
}

void fpsPlayer::stop() {}
