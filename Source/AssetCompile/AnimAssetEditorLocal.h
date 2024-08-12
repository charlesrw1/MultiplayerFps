#pragma once
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


class EventTimelineSequencer : public SequencerImgui
{
public:
	void context_menu_callback() override;
};
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
	EventTimelineSequencer seqimgui;
	PropertyGrid eventdetails;
	float CURRENT_TIME = 0.0;
};

class StaticMeshEntity;
class AnimationEditorTool : public IEditorTool
{
public:
	AnimationEditorTool() {
		animEdit = std::make_unique<EditModelAnimations>();
	}

	void draw_menu_bar() override;
	// Inherited via IEditorTool

	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override;
	virtual void overlay_draw() override;
	virtual void init() override;
	virtual bool can_save_document() override;
	virtual const char* get_editor_name() override;
	virtual bool has_document_open() const override;
	virtual void open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	void on_open_map_callback(bool good);

	View_Setup view;
	User_Camera camera;
	//AnimatorInstance animator;

	PropertyGrid propGrid;
	StaticMeshEntity* outputEntity = nullptr;
	Model* outputModel = nullptr;
	ModelImportSettings* importSettings = nullptr;
	AnimImportSettings* animImportSettings = nullptr;
	const AnimationSeq* sequence = nullptr;
	std::unique_ptr<EditModelAnimations> animEdit;

	MulticastDelegate<> on_pre_save;
	MulticastDelegate<> on_post_save;
	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
};
