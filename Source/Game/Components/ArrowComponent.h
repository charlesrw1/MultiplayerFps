#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Render/Model.h"
#include "Render/DrawPublic.h"
#include "Assets/AssetDatabase.h"
#include "Render/RenderObj.h"
#include "Game/Entity.h"
class ArrowComponent : public Component
{
public:
	CLASS_BODY(ArrowComponent);

	ArrowComponent() {
		set_call_init_in_editor(true);
		arrowModel = g_assets.find_assetptr_unsafe<Model>("arrowModel.cmdl");
		dont_serialize_or_edit = true;	// default to true
	}

	void start() override {
		sync_render_data();
	}
	void end() override {
		idraw->get_scene()->remove_obj(handle);
	}
	void on_changed_transform() override {
		sync_render_data();
	}
	void on_sync_render_data() final {
		if(!handle.is_valid())
			handle = idraw->get_scene()->register_obj();
		Render_Object obj;
		obj.model = arrowModel.get();
		obj.transform = get_ws_transform();
		obj.visible = true;
#ifdef  EDITOR_BUILD
		obj.visible &= !get_owner()->get_hidden_in_editor();
		obj.outline = get_owner()->get_is_any_selected_in_editor();
#endif //  EDITOR_BUILD
		obj.owner = this;
		idraw->get_scene()->update_obj(handle, obj);
	}
private:
	AssetPtr<Model> arrowModel;
	handle<Render_Object> handle;
};