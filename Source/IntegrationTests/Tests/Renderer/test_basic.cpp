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

// ssr/taa smoke test
static TestTask test_ssr_motion_vec_smoke(TestContext& t) {
	r_taa_enabled.set_bool(true);
	r_taa_enabled.set_bool(false);
	auto pre_size = get_app_window_size();
	change_map_and_make_camera("demo_level_1.tmap", {});

	for (int i = 0; i < 5; i++) {
		set_app_window_size({ pre_size.x + i * 15,pre_size.y });
		co_await t.wait_ticks(1);
	}
	co_await t.capture_screenshot("ssr_smoke");
	set_app_window_size(pre_size);
}
GAME_TEST("renderer/ssr_smoke", 15.f, test_ssr_motion_vec_smoke);
