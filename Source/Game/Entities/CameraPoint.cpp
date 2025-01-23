
#include "CameraPoint.h"
#include "Game/Components/BillboardComponent.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ArrowComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/MeshbuilderComponent.h"
#include "Render/Frustum.h"

CLASS_IMPL(CameraPoint);
View_Setup cursed_view_setup{};
CLASS_H(FakeCameraComponent, EntityComponent)
public:
	FakeCameraComponent() {
		dont_serialize_or_edit = true;
		set_call_init_in_editor(true);
	}
	void start() override {
		mbview = get_owner()->create_and_attach_component_type<MeshBuilderComponent>();
		mbview->use_transform = false;
		mbview->use_background_color = true;
		update_meshbuilder();
	}
	void on_changed_transform() override {
		update_meshbuilder();
	}
	void update_meshbuilder() {
		Frustum f;
		assert(get_owner()->is_a<CameraPoint>());
		auto fov = get_owner()->cast_to<CameraPoint>()->fov;

		auto view = glm::inverse(get_ws_transform());
		View_Setup vs = View_Setup(view, glm::radians(fov), 0.01, 100.0, 1280, 720);

		glm::vec3 arrow_origin[4];
		build_a_frustum_for_perspective(f, vs, arrow_origin);

		auto& mb = mbview->mb;
		mb.Begin();
		mb.PushLine(arrow_origin[0], arrow_origin[0] + glm::vec3(f.right_plane), COLOR_RED);
		mb.PushLine(arrow_origin[1], arrow_origin[1] + glm::vec3(f.top_plane), COLOR_GREEN);
		mb.PushLine(arrow_origin[2], arrow_origin[2] + glm::vec3(f.left_plane), COLOR_BLUE);
		mb.PushLine(arrow_origin[3], arrow_origin[3] + glm::vec3(f.bot_plane), COLOR_PINK);


		mb.PushLine(vs.origin, arrow_origin[0], COLOR_RED);
		mb.PushLine(vs.origin, arrow_origin[1], COLOR_GREEN);
		mb.PushLine(vs.origin, arrow_origin[2], COLOR_BLUE);
		mb.PushLine(vs.origin, arrow_origin[3], COLOR_PINK);

		mb.End();

		cursed_view_setup = vs;
	}

	MeshBuilderComponent* mbview = nullptr;
};
CLASS_IMPL(FakeCameraComponent);

CameraPoint::CameraPoint()
{
	if (eng->is_editor_level()) {

		auto m = construct_sub_component<MeshComponent>("CameraModel");
		m->set_model(GetAssets().find_global_sync<Model>("camera_model.cmdl").get());
		m->dont_serialize_or_edit = true;

		construct_sub_component<FakeCameraComponent>("EditorViewier");
	}
}