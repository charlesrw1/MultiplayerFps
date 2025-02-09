#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/StdEntityTypes.h"
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"
CLASS_H(TestEntityWithSubEntities, Entity)
public:
	TestEntityWithSubEntities() {

		auto cube = g_assets.find_assetptr_unsafe<Model>("bike.cmdl");
		auto cone = g_assets.find_assetptr_unsafe<Model>("cone.cmdl");


		auto m = construct_sub_component<MeshComponent>("main");
		m->set_model(cube);
		
		auto light_obj = construct_sub_entity<SpotLightEntity>("spot");
		light_obj->set_ws_position(glm::vec3(0, 2, 0));
		
		auto other = construct_sub_entity<Entity>("another");
		other->construct_sub_component<MeshComponent>("mesh")->set_model(cone);
		other->set_ws_position(glm::vec3(1, 0, 0));
	}
	static const PropertyInfoList* get_props() = delete;
};

CLASS_IMPL(TestEntityWithSubEntities);