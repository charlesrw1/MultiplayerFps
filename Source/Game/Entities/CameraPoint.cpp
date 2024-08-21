
#include "CameraPoint.h"
#include "Game/Components/BillboardComponent.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ArrowComponent.h"
CLASS_IMPL(CameraPoint);

CameraPoint::CameraPoint()
{
	if (eng->is_editor_level()) {
		auto billboard = create_sub_component<BillboardComponent>("Editorbillboard");
		billboard->set_texture(GetAssets().find_global_sync<Texture>("icon/cameraBig.png").get());
		billboard->dont_serialize_or_edit = true;

		auto arrow = create_sub_component<ArrowComponent>("Arrow");
		arrow->dont_serialize_or_edit = true;
	}
}