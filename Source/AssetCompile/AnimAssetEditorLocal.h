#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Render/DrawPublic.h"
#include "Types.h"

#include "Framework/CurveEditorImgui.h"
#include "Framework/PropertyEd.h"

#include "Animation/Event.h"

#include "Animation/SkeletonData.h"

#include "ModelAsset2.h"
#include "Framework/MulticastDelegate.h"
class Model;

class Animation_Tree_CFG;


class EventSequenceItem : public SequencerEditorItem
{
public:
	EventSequenceItem(AnimationEvent* ev) : event(ev) {
		color = ev->get_editor_color();
		instant_item = !ev->is_duration_event();
	}
	virtual std::string get_name() { return event->get_type().classname; };

	std::unique_ptr<AnimationEvent> event;
};

class EditModelAnimations
{
public:
	EditModelAnimations();
	void on_start();
	void on_quit();
	void on_presave();
	void on_postsave();

	void draw_imgui();
	void add_to_obj(Render_Object& obj, float dt);

	const EventSequenceItem* selected_event_item = nullptr;


	CurveEditorImgui curveedit;
	PropertyGrid eventdetails;
	float CURRENT_TIME = 0.0;
};
#include "Animation/Runtime/Animation.h"

#include "EditorTool3d.h"
class MeshComponent;
class StaticMeshEntity;
class AnimationEditorTool : public EditorTool3d
{
public:
	AnimationEditorTool() {
		animEdit = std::make_unique<EditModelAnimations>();
	}

	// Inherited via IEditorTool
	void tick(float dt) override;
	const ClassTypeInfo& get_asset_type_info() const override {
		return AnimationSeqAsset::StaticType;
	}

	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void post_map_load_callback() override;
	void imgui_draw() override;

	const char* get_save_file_extension() const {
		return "NONE";
	}

	Entity* entity = nullptr;
	MeshComponent* mc = nullptr;
	Model* outputModel{};
	std::unique_ptr<Animation_Tree_CFG> fake_tree;

	PropertyGrid propGrid;
	ModelImportSettings* importSettings = nullptr;
	AnimImportSettings* animImportSettings = nullptr;
	const AnimationSeqAsset* sequence = nullptr;
	std::unique_ptr<EditModelAnimations> animEdit;

	MulticastDelegate<> on_pre_save;
	MulticastDelegate<> on_post_save;
	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
};
#endif