#include "WorldSettings.h"
#include "Game/Components/BillboardComponent.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"

CLASS_IMPL(WorldSettings);
WorldSettings::WorldSettings()
{
	if (eng->is_editor_level()) {
		auto b = create_sub_component<BillboardComponent>("Billboard");
		b->set_texture(default_asset_load<Texture>("icon/_nearest/worldsettings.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	}
}