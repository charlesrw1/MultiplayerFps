#pragma once
#include "IEditorTool.h"

#include "DataClass.h"

#include "Framework/PropertyEd.h"

class DataClassEditor : public IEditorTool
{
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
	void draw_menu_bar() override;
	void imgui_draw() override;
	std::string get_save_root_dir() override { return "./Data/"; }

	PropertyGrid grid;
	std::string dc_name;
	ClassBase* editing_object = nullptr;
};