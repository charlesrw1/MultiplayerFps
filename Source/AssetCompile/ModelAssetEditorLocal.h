#pragma once
#include "IEditorTool.h"
#include "DrawPublic.h"
class ModelEditorTool : public IEditorTool
{
	// Inherited via IEditorTool
	virtual void ui_paint() override;
	virtual bool handle_event(const SDL_Event& event) override;
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

	View_Setup view;
};
