#include "EditorTool3d.h"
#ifdef EDITOR_BUILD
#include <SDL2/SDL.h>
#include "GameEnginePublic.h"
#include "OsInput.h"
#include "Framework/MulticastDelegate.h"
#include "Game/StdEntityTypes.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "Render/MaterialPublic.h"
#include "Level.h"
#include "Render/ModelManager.h"

extern ConfigVar ed_default_sky_material;

void EditorTool3d::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	const float fov = glm::radians(70.f);
	{
		int x = 0, y = 0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			camera.update_from_input(eng->get_input_state()->keys, x, y,window_sz.x,window_sz.y, aratio,fov);
		}
	}
	view = View_Setup(camera.position, camera.front, fov, 0.01, 100.0, window_sz.x, window_sz.y);
}
void EditorTool3d::close_internal()
{
	skyEntity = planeEntity = nullptr;
	eng->leave_level();
	eng->get_on_map_delegate().remove(this);
}
bool EditorTool3d::open_document_internal(const char* name, const char* arg) 
{
	eng->open_level("__empty__");
	eng->get_on_map_delegate().add(this, &EditorTool3d::map_callback);
	return true;
}
void EditorTool3d::map_callback(bool b)
{
	assert(b);

	auto dome = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
	dome->set_model(g_assets.find_sync<Model>("eng/skydome.cmdl").get());
	dome->get_owner()->set_ls_transform(glm::vec3(0), {}, glm::vec3(10000.0));
	dome->set_is_skybox( true );	// FIXME
	dome->set_casts_shadows( false );
	//dome->Mesh->set_material_override(g_assets.find_sync<MaterialInstance>(ed_default_sky_material.get_string()).get());

	auto plane = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
	plane->set_model(g_modelMgr.get_default_plane_model());
	plane->get_owner()->set_ws_transform({}, {}, glm::vec3(20.f));
	plane->set_material_override((g_assets.find_sync<MaterialInstance>("eng/defaultWhite.mi").get()));

	auto sun = eng->get_level()->spawn_entity()->create_component<SunLightComponent>();
	sun->intensity = 2.0;
	sun->visible = true;
	sun->log_lin_lerp_factor = 0.7;
	sun->max_shadow_dist = 40.0;
	sun->get_owner()->set_ls_euler_rotation(glm::vec3(-glm::radians(45.f), glm::radians(15.f), 0.f));

	// i dont expose skylight through a header, could change that or just do this (only meant to be spawned by the level editor)
	auto skylight = eng->get_level()->spawn_entity()->create_component<SkylightComponent>();


	post_map_load_callback();
}

#endif