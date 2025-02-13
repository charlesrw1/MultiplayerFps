#include "Game/EntityComponent.h"
#include "Render/Texture.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/RenderFog.h"
#include "Framework/ReflectionMacros.h"
#include "GameEnginePublic.h"

CLASS_H(FogComponent, EntityComponent)
public:
	FogComponent() {
		set_call_init_in_editor(true);
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(FogComponent)
			//REG_INT_W_CUSTOM(f.inscattering_color,PROP_DEFAULT,"","ColorUint"),
			//REG_FLOAT(f.fog_density, PROP_DEFAULT, "1.0"),
			REG_FLOAT(height_offset,PROP_DEFAULT,"0.0"),
			//REG_FLOAT(f.fog_height_falloff,PROP_DEFAULT,"1.0"),
			//REG_INT_W_CUSTOM(f.directional_color, PROP_DEFAULT, "", "ColorUint"),
			//REG_FLOAT(f.directional_exponent, PROP_DEFAULT, "1.0"),

		END_PROPS(FogComponent)
	}
	void on_sync_render_data() final {
		if (!handle.is_valid())
			handle = idraw->get_scene()->register_fog();
		RenderFog fog;
		idraw->get_scene()->update_fog(handle, fog);
	}

	void start() final {
		sync_render_data();
	}
	void end() final {
		idraw->get_scene()->remove_fog(handle);
	}
	void on_changed_transform() final {
		sync_render_data();
	}
	void editor_on_change_property()final {
		sync_render_data();
	}

	handle<RenderFog> handle;
	float height_offset = 0.0;
};
CLASS_IMPL(FogComponent);
#include "BillboardComponent.h"
#include "Assets/AssetDatabase.h"
CLASS_H(FogEntity, Entity)
public:
	FogEntity() {
		Fog = construct_sub_component<FogComponent>("Fog");

		if (eng->is_editor_level()) {
			auto b = construct_sub_component<BillboardComponent>("Billboard");
			b->set_texture(default_asset_load<Texture>("icon/_nearest/fog.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		}
	}
	const PropertyInfoList* get_props() = delete;

	FogComponent* Fog{};
};
CLASS_IMPL(FogEntity);