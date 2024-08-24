#include "EditorTool3d.h"
#include <SDL2/SDL.h>
#include "GameEnginePublic.h"
#include "OsInput.h"
#include "Framework/MulticastDelegate.h"
#include "Game/StdEntityTypes.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "Render/MaterialPublic.h"

extern ConfigVar ed_default_sky_material;

void EditorTool3d::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x = 0, y = 0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			camera.update_from_input(eng->get_input_state()->keys, x, y, glm::mat4(1.f));
		}
	}
	view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
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

	auto dome = eng->spawn_entity_class<StaticMeshEntity>();
	dome->Mesh->set_model(GetAssets().find_sync<Model>("skydome.cmdl").get());
	dome->Mesh->set_ls_transform(glm::vec3(0), {}, glm::vec3(10000.0));
	dome->Mesh->is_skybox = true;	// FIXME
	dome->Mesh->cast_shadows = false;
	dome->Mesh->set_material_override(GetAssets().find_sync<MaterialInstance>(ed_default_sky_material.get_string()).get());

	auto plane = eng->spawn_entity_class<StaticMeshEntity>();
	plane->Mesh->set_model(mods.get_default_plane_model());
	plane->set_ws_transform({}, {}, glm::vec3(20.f));
	plane->Mesh->set_material_override((GetAssets().find_sync<MaterialInstance>("defaultWhite").get()));

	auto sun = eng->spawn_entity_class<SunLightEntity>();
	sun->Sun->intensity = 3.0;
	sun->Sun->visible = true;
	sun->Sun->log_lin_lerp_factor = 0.8;
	sun->Sun->max_shadow_dist = 40.0;
	sun->Sun->set_ls_euler_rotation(glm::vec3(-glm::radians(45.f), glm::radians(15.f), 0.f));

	// i dont expose skylight through a header, could change that or just do this (only meant to be spawned by the level editor)
	auto skylight = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));


	post_map_load_callback();
}

