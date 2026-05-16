// Source/IntegrationTests/Tests/Renderer/test_basic.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
static void change_map_and_make_camera(const std::string& path, glm::vec3 pos)
{
	eng->load_level(path);
	auto cc = eng->get_level()->spawn_entity()->create_component<CameraComponent>();
	cc->set_is_enabled(true);
	cc->get_owner()->set_ws_position(pos);
	ASSERT(CameraComponent::get_scene_camera() == cc);
}


#include "Render/RenderConfigVars.h"
