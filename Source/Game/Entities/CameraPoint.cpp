
#include "CameraPoint.h"
#include "Game/Components/BillboardComponent.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ArrowComponent.h"
#include "Game/Components/MeshComponent.h"
CLASS_IMPL(CameraPoint);

CameraPoint::CameraPoint()
{
	if (eng->is_editor_level()) {

		auto m = create_sub_component<MeshComponent>("CameraModel");
		m->set_model(GetAssets().find_global_sync<Model>("camera_model.cmdl").get());
		m->dont_serialize_or_edit = true;
	}
}