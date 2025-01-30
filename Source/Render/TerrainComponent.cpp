#include "Game/EntityComponent.h"
#include "Render/Texture.h"
#include "MaterialPublic.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/TerrainPublic.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "GameEnginePublic.h"
CLASS_H(TerrainComponent,EntityComponent)
public:
	AssetPtr<Texture> heightmap;
	AssetPtr<MaterialInstance> terrain_material;
	float vertical_scale = 10.0;
	float width = 100.0;

	int min_tess_level = 4;
	int max_tess_level = 40;
	float min_distance = 0.5;
	float max_distance = 80;


	handle<Render_Terrain> handle;
	static const PropertyInfoList* get_props() {
		START_PROPS(TerrainComponent)
			REG_ASSET_PTR(heightmap, PROP_DEFAULT),
			REG_ASSET_PTR(terrain_material, PROP_DEFAULT),
			REG_FLOAT(vertical_scale, PROP_DEFAULT, "10.0"),
			REG_FLOAT(width, PROP_DEFAULT, "100.0"),
			REG_FLOAT(min_distance, PROP_DEFAULT, "0.5"),
			REG_FLOAT(max_distance, PROP_DEFAULT, "80.0"),
			REG_INT(min_tess_level, PROP_DEFAULT, "4"),
			REG_INT(max_tess_level, PROP_DEFAULT, "40"),
		END_PROPS(TerrainComponent)
	}
	
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
	virtual void on_init() {

		Render_Terrain rt = make_terrain();
		handle = idraw->get_scene()->get_terrain_interface()->register_terrain(rt);
	}
	// called when component is being removed, remove all handles
	virtual void on_deinit() {
		idraw->get_scene()->get_terrain_interface()->remove_terrain(handle);
	}

	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {
		Render_Terrain rt = make_terrain();
		idraw->get_scene()->get_terrain_interface()->update_terrain(handle, rt);
	}
};

CLASS_IMPL(TerrainComponent);
#include "Game/Components/BillboardComponent.h"
#include "Assets/AssetDatabase.h"
CLASS_H(TerrainEntity, Entity)
public:
	TerrainEntity() {
		Terrain = construct_sub_component<TerrainComponent>("Terrain");

		if (eng->is_editor_level()) {
			auto b = construct_sub_component<BillboardComponent>("Billboard");
			b->set_texture(default_asset_load<Texture>("icon/_nearest/terrain.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		}
	}
	static const PropertyInfoList* get_props() = delete;
	TerrainComponent* Terrain{};
};
CLASS_IMPL(TerrainEntity);