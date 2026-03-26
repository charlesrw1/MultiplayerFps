#pragma once
#include "Game/EntityComponent.h"
#include "Render/RenderObj.h"
#include "Render/DrawPublic.h"
#include "Framework/MeshBuilder.h"

class MeshBuilderComponent : public Component
{
public:
	CLASS_BODY(MeshBuilderComponent);

	MeshBuilderComponent() {
		dont_serialize_or_edit = true;
		set_call_init_in_editor(true);
	}
	~MeshBuilderComponent() { assert(!editor_mb_handle.is_valid()); }

	void start() override { sync_render_data(); }
	void stop() override { idraw->get_scene()->remove_meshbuilder(editor_mb_handle); }
	void on_changed_transform() override { sync_render_data(); }
	void on_sync_render_data() final;

	Color32 background_color = COLOR_BLACK;
	bool use_background_color = false;
	bool use_transform = true;
	bool depth_tested = true;
	handle<MeshBuilder_Object> editor_mb_handle;
	MeshBuilder mb;
};
class GuiFont;
using std::string;
struct Particle_Object;
#include "Render/DynamicMaterialPtr.h"
class WorldTextComponent : public Component
{
public:
	CLASS_BODY(WorldTextComponent);
	WorldTextComponent();
	~WorldTextComponent();
	void start() final;
	void stop() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;

	DynamicMatUniquePtr font_mat;
	handle<Particle_Object> editor_mb_handle;
	MeshBuilder mb;
	REF GuiFont* font = nullptr;
	REF string text;
};