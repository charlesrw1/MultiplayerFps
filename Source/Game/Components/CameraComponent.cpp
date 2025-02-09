#include "CameraComponent.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Game/Components/MeshbuilderComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "Render/Frustum.h"


CLASS_IMPL(CameraComponent);

CameraComponent* CameraComponent::scene_camera;

void CameraComponent::start()
{
	if (eng->is_editor_level()) {

		editor_mesh = get_owner()->create_component<MeshComponent>();
		editor_mesh->set_model(g_assets.find_global_sync<Model>("camera_model.cmdl").get());
		editor_mesh->dont_serialize_or_edit = true;

		editor_mbview = get_owner()->create_component<MeshBuilderComponent>();
		editor_mbview->dont_serialize_or_edit = true;
		editor_mbview->use_transform = false;
		editor_mbview->use_background_color = true;

		update_meshbuilder();
	}
}
void CameraComponent::end()
{
	if (editor_mbview)
		editor_mbview->destroy();
	if (editor_mesh)
		editor_mesh->destroy();

	if (scene_camera == this)
		scene_camera = nullptr;
}

void CameraComponent::update_meshbuilder() {
	Frustum f;

	auto view = glm::inverse(get_ws_transform());
	View_Setup vs = View_Setup(view, glm::radians(fov), 0.01, 100.0, 1280, 720);

	glm::vec3 arrow_origin[4];
	build_a_frustum_for_perspective(f, vs, arrow_origin);

	auto& mb = editor_mbview->mb;
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

	editor_mbview->on_changed_transform();
}
void CameraComponent::get_view(glm::mat4& viewMatrix, float& fov)
{
	fov = this->fov;
	viewMatrix = glm::inverse(get_ws_transform());
}