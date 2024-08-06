#pragma once
#include "IEditorTool.h"
#include "Render/DrawPublic.h"
#include "Types.h"

#include "Framework/CurveEditorImgui.h"
#include "Framework/PropertyEd.h"

#include "ModelCompilierLocal.h"
#include "Animation/Runtime/Animation.h"
class Model;
// defines 
enum class ModelEditType
{
	General,
	Animation,
	Physics,
};

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
	EditModelAnimations() {
		name_filter[0] = 0;
	}
	void draw_imgui();
	void add_to_obj(Render_Object& obj, float dt);

	void init_from_model(const Model* m);
	void on_select_new_animation(int next);

	struct ALdat {
		std::string name;
		const AnimationSeq* seq = nullptr;
	};
	std::vector<ALdat> names;
	const EventSequenceItem* selected_event_item = nullptr;

	int selected_index = -1;
	AnimationClip_Load& find_or_create_for_selected();

	CurveEditorImgui curveedit;
	EventTimelineSequencer seqimgui;
	PropertyGrid eventdetails;

	char name_filter[256];
	float CURRENT_TIME = 0.0;

	AnimatorInstance animator;


};

class ModelEditorTool : public IEditorTool
{
public:
	void draw_menu_bar() override;
	// Inherited via IEditorTool

	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override;
	virtual void overlay_draw() override;
	virtual void on_change_focus(editor_focus_state newstate) override;
	virtual void init() override;
	virtual bool can_save_document() override;
	virtual const char* get_editor_name() override;
	virtual bool has_document_open() const override;
	virtual void open_document_internal(const char* name) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	bool is_open = false;
	View_Setup view;
	User_Camera camera;

	 Model* compilied_model = nullptr;

	unique_ptr<ModelDefData> model_def;
	handle<Render_Object> object;

	void hide();

	EditModelAnimations editanims;

	void draw_imgui_anim_state();

	ModelEditType edit_state_type = ModelEditType::General;
};
