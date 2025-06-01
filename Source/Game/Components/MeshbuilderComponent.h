#pragma once
#include "Game/EntityComponent.h"
#include "Render/RenderObj.h"
#include "Render/DrawPublic.h"
#include "Framework/MeshBuilder.h"

CLASS_H(MeshBuilderComponent, Component)
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
	}
	void on_changed_transform() override {
		sync_render_data();
	}
	void on_sync_render_data() final;

	Color32 background_color = COLOR_BLACK;
	bool use_background_color = false;
	bool use_transform = true;
	bool depth_tested = true;
	handle<MeshBuilder_Object> editor_mb_handle;
	MeshBuilder mb;
};