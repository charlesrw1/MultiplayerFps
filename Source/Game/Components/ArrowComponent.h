#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Render/Model.h"
#include "Render/DrawPublic.h"
#include "Assets/AssetDatabase.h"
#include "Render/RenderObj.h"
#include "Game/Entity.h"
CLASS_H(ArrowComponent,EntityComponent)
public:
	ArrowComponent() {
		set_call_init_in_editor(true);
		arrowModel = g_assets.find_assetptr_unsafe<Model>("arrowModel.cmdl");
		dont_serialize_or_edit = true;	// default to true
	}

	void start() override {
		renderable = idraw->get_scene()->register_obj();
		update_object();
	}
	void end() override {
		idraw->get_scene()->remove_obj(renderable);
	}
	void on_changed_transform() override {
		update_object();
	}
	void update_object() {
		Render_Object obj;
		obj.model = arrowModel.get();
		obj.transform = get_ws_transform();
		obj.visible = true;
		obj.outline = get_owner()->is_selected_in_editor();
		obj.owner = this;
		idraw->get_scene()->update_obj(renderable, obj);
	}
private:
	AssetPtr<Model> arrowModel;
	handle<Render_Object> renderable;
};