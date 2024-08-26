#include "Game/EntityComponent.h"
#include "Render/Texture.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/RenderFog.h"

CLASS_H(FogComponent, EntityComponent)
public:

	static const PropertyInfoList* get_props() {
		START_PROPS(FogComponent)
			REG_INT_W_CUSTOM(f.inscattering_color,PROP_DEFAULT,"","ColorUint"),
			REG_FLOAT(f.fog_density, PROP_DEFAULT, "1.0"),
			REG_FLOAT(height_offset,PROP_DEFAULT,"0.0"),
			REG_FLOAT(f.fog_height_falloff,PROP_DEFAULT,"1.0"),
			REG_INT_W_CUSTOM(f.directional_color, PROP_DEFAULT, "", "ColorUint"),
			REG_FLOAT(f.directional_exponent, PROP_DEFAULT, "1.0"),

		END_PROPS(FogComponent)
	}

	void on_init() {
		f.height = get_ws_position().y + height_offset;
		handle = idraw->get_scene()->register_fog(f);
	}
	void on_deinit() {
		idraw->get_scene()->remove_fog(handle);
	}
	void on_changed_transform() {
		f.height = get_ws_position().y + height_offset;
		idraw->get_scene()->update_fog(handle,f);
	}
	void editor_on_change_property() {
		idraw->get_scene()->update_fog(handle, f);
	}

	handle<RenderFog> handle;
	float height_offset = 0.0;
	RenderFog f;
};
CLASS_IMPL(FogComponent);
#include "BillboardComponent.h"
#include "Assets/AssetDatabase.h"
CLASS_H(FogEntity, Entity)
public:
	FogEntity() {
		Fog = create_sub_component<FogComponent>("Fog");
		root_component = Fog;

		if (eng->is_editor_level()) {
			auto b = create_sub_component<BillboardComponent>("Billboard");
			b->set_texture(default_asset_load<Texture>("icon/_nearest/fog.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		}
	}
	const PropertyInfoList* get_props() = delete;

	FogComponent* Fog{};
};
CLASS_IMPL(FogEntity);