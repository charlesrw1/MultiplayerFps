#include "MeshbuilderComponent.h"
#include "Game/Entity.h"
// comment 4

void MeshBuilderComponent::on_sync_render_data()  {
	if (!editor_mb_handle.is_valid())
		editor_mb_handle = idraw->get_scene()->register_meshbuilder();
	MeshBuilder_Object obj;
	obj.depth_tested = depth_tested;
	obj.owner = this;
	if (use_transform)
		obj.transform = get_ws_transform();
	else
		obj.transform = glm::mat4(1);
	obj.visible = true;
#ifdef  EDITOR_BUILD
	obj.visible &= !get_owner()->get_hidden_in_editor();
#endif //  EDITOR_BUILD
	obj.use_background_color = use_background_color;
	obj.background_color = background_color;
	obj.meshbuilder = &mb;
	idraw->get_scene()->update_meshbuilder(editor_mb_handle, obj);
}