#include "Game/EntityComponent.h"
#include "Render/Texture.h"
#include "MaterialPublic.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/TerrainPublic.h"
#include "Framework/ReflectionMacros.h"

#include "GameEnginePublic.h"

#include "Game/Components/BillboardComponent.h"
#include "Assets/AssetDatabase.h"
class TerrainComponent : public Component {
public:
	CLASS_BODY(TerrainComponent);
	REF AssetPtr<Texture> heightmap;
	REF AssetPtr<MaterialInstance> terrain_material;
	REF float vertical_scale = 10.0;
	REF float width = 100.0;
	REF int min_tess_level = 4;
	REF int max_tess_level = 40;
	REF float min_distance = 0.5;
	REF float max_distance = 80;


	handle<Render_Terrain> handle;

	
	Render_Terrain make_terrain() {
		Render_Terrain rt;
		rt.assetptr_material = terrain_material.get();
		rt.assetptr_heightfield = heightmap.get();
		rt.vertical_scale = vertical_scale;
		rt.dimensions = width;
		rt.min_distance = min_distance;
		rt.max_distance = max_distance;
		rt.min_tess_level = min_tess_level;
		rt.max_tess_level = max_tess_level;
		return rt;
	}

	void editor_on_change_property() override {
		Render_Terrain rt = make_terrain();
		idraw->get_scene()->get_terrain_interface()->update_terrain(handle, rt);
	}

	// use to get handles, setup state
	void start() final {

		if (eng->is_editor_level()) {
			auto b = get_owner()->create_component<BillboardComponent>();
			b->set_texture(default_asset_load<Texture>("icon/_nearest/terrain.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		}

		Render_Terrain rt = make_terrain();
		handle = idraw->get_scene()->get_terrain_interface()->register_terrain(rt);
	}
	// called when component is being removed, remove all handles
	void end() final {
		idraw->get_scene()->get_terrain_interface()->remove_terrain(handle);
	}

	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	void on_changed_transform() final {
		Render_Terrain rt = make_terrain();
		idraw->get_scene()->get_terrain_interface()->update_terrain(handle, rt);
	}
};
