#pragma once
#include "Game/EntityComponent.h"
#include "Render/RenderObj.h"
#include "Render/DrawPublic.h"
#include "Framework/MeshBuilder.h"

CLASS_H(MeshBuilderComponent, EntityComponent)
public:
	MeshBuilderComponent() {
		dont_serialize_or_edit = true;
		set_call_init_in_editor(true);
	}
	~MeshBuilderComponent() {
		assert(!editor_mb_handle.is_valid());
	}

	static const PropertyInfoList* get_props() = delete;
	void start() override {
		sync_render_data();
	}
	void end() override {
		idraw->get_scene()->remove_meshbuilder(editor_mb_handle);
		mb.Free();
	}
	void on_changed_transform() override {
		sync_render_data();
	}
	void on_sync_render_data() final {
		if(!editor_mb_handle.is_valid())
			editor_mb_handle = idraw->get_scene()->register_meshbuilder();
		MeshBuilder_Object obj;
		obj.depth_tested = depth_tested;
		obj.owner = this;
		if (use_transform)
			obj.transform = get_ws_transform();
		else
			obj.transform = glm::mat4(1);
		obj.visible = true;
		obj.use_background_color = use_background_color;
		obj.background_color = background_color;
		obj.meshbuilder = &mb;
		idraw->get_scene()->update_meshbuilder(editor_mb_handle, obj);
	}

	Color32 background_color = COLOR_BLACK;
	bool use_background_color = false;
	bool use_transform = true;
	bool depth_tested = true;
	handle<MeshBuilder_Object> editor_mb_handle;
	MeshBuilder mb;
};