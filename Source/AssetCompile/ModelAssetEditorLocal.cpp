#include "AssetCompile/ModelAssetEditorLocal.h"

void ModelEditorTool::ui_paint()
{
}

bool ModelEditorTool::handle_event(const SDL_Event& event)
{
	return false;
}

void ModelEditorTool::tick(float dt)
{
}

const View_Setup& ModelEditorTool::get_vs()
{
	// TODO: insert return statement here
	return {};
}

void ModelEditorTool::overlay_draw()
{
}

void ModelEditorTool::on_change_focus(editor_focus_state newstate)
{
}

void ModelEditorTool::init()
{
}

bool ModelEditorTool::can_save_document()
{
	return false;
}

const char* ModelEditorTool::get_editor_name()
{
	return nullptr;
}

bool ModelEditorTool::has_document_open() const
{
	return false;
}

void ModelEditorTool::open_document_internal(const char* name)
{
}

void ModelEditorTool::close_internal()
{
}

bool ModelEditorTool::save_document_internal()
{
	return false;
}
