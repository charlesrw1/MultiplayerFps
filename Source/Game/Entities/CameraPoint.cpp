
#include "CameraPoint.h"
#include "Game/Components/BillboardComponent.h"
#include "Render/Texture.h"
CLASS_IMPL(CameraPoint);

CameraPoint::CameraPoint()
{
	if (eng->is_editor_level()) {
		auto billboard = create_sub_component<BillboardComponent>("Editorbillboard");
		billboard->set_texture(g_imgs.find_texture("icon/cameraBig.png"));
		billboard->dont_serialize_or_edit = true;
	}
}