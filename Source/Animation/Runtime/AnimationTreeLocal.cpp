
#include "Framework/AddClassToFactory.h"
#include "Assets/AssetRegistry.h"

#include "Framework/PoolAllocator.h"
#include "Animation.h"

#include "Game/Components/MeshComponent.h"
#include "GameEnginePublic.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Render/ModelManager.h"
#include "Game/Components/LightComponents.h"
#include "Render/MaterialPublic.h"
#include "AnimationTreeLocal.h"

void post_load_map_callback_generic(bool make_plane)
{
	auto dome = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
	dome->set_model(g_assets.find_sync<Model>("eng/skydome.cmdl").get());
	dome->get_owner()->set_ls_transform(glm::vec3(0), {}, glm::vec3(10000.0));
	dome->set_is_skybox(true);	// FIXME
	dome->set_casts_shadows(false);
	//dome->Mesh->set_material_override(g_assets.find_sync<MaterialInstance>(ed_default_sky_material.get_string()).get());

	if (make_plane) {
		auto plane = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
		plane->set_model(g_modelMgr.get_default_plane_model());
		plane->get_owner()->set_ws_transform({}, {}, glm::vec3(20.f));
		plane->set_material_override((g_assets.find_sync<MaterialInstance>("eng/defaultWhite.mi").get()));
	}

	auto sun = eng->get_level()->spawn_entity()->create_component<SunLightComponent>();
	sun->intensity = 2.0;
	sun->visible = true;
	sun->log_lin_lerp_factor = 0.7;
	sun->max_shadow_dist = 40.0;
	sun->get_owner()->set_ls_euler_rotation(glm::vec3(-glm::radians(45.f), glm::radians(15.f), 0.f));
	auto skylight = eng->get_level()->spawn_entity()->create_component<SkylightComponent>();

}

Pool_Allocator<Pose> g_pose_pool = Pool_Allocator<Pose>(100, "g_pose_pool");
Pool_Allocator<MatrixPose> g_matrix_pool = Pool_Allocator<MatrixPose>(10,"g_matrix_pool");
