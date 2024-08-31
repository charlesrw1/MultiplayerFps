#pragma once
#include "Game/EntityComponent.h"
#include "Render/RenderObj.h"
#include "Render/DrawPublic.h"
#include "Framework/MeshBuilder.h"

CLASS_H(MeshBuilderComponent, EntityComponent)
public:
	MeshBuilderComponent() {
		dont_serialize_or_edit = true;
	}
	~MeshBuilderComponent() {
		assert(!editor_mb_handle.is_valid());
	}

	static const PropertyInfoList* get_props() = delete;
	void on_init() override {
		MeshBuilder_Object mbo;
		fill_out_struct(mbo);
		editor_mb_handle = idraw->get_scene()->register_meshbuilder(mbo);
	}
	void on_deinit() override {
		idraw->get_scene()->remove_meshbuilder(editor_mb_handle);
		editor_mb.Free();
	}
	void on_changed_transform() override {
		MeshBuilder_Object mbo;
		fill_out_struct(mbo);
		idraw->get_scene()->update_meshbuilder(editor_mb_handle, mbo);
	}
	void fill_out_struct(MeshBuilder_Object& obj) {
		obj.depth_tested = true;
		obj.owner = this;
		if (use_transform)
			obj.transform = get_ws_transform();
		else
			obj.transform = glm::mat4(1);
		obj.visible = true;
		obj.meshbuilder = &editor_mb;
	}

	bool use_transform = true;
	handle<MeshBuilder_Object> editor_mb_handle;
	MeshBuilder editor_mb;
};