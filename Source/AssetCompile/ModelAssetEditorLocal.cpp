#include "AssetCompile/ModelAssetEditorLocal.h"
#include "Game_Engine.h"

static ModelEditorTool g_model_editor_static;
IEditorTool* g_model_editor = &g_model_editor_static;

void ModelEditorTool::ui_paint()
{
}

bool ModelEditorTool::handle_event(const SDL_Event& event)
{
	return false;
}

void ModelEditorTool::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_dimensions();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x = 0, y = 0;
		if (eng->get_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
				camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
		}
	}
	view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}

const View_Setup& ModelEditorTool::get_vs()
{
	// TODO: insert return statement here
	return view;
}

void ModelEditorTool::overlay_draw()
{
}

void ModelEditorTool::on_change_focus(editor_focus_state newstate)
{
	if (newstate == editor_focus_state::Closed)
		close();
}

void ModelEditorTool::init()
{
}

bool ModelEditorTool::can_save_document()
{
	return true;
}

const char* ModelEditorTool::get_editor_name()
{
	return "Model Editor";
}

bool ModelEditorTool::has_document_open() const
{
	return is_open;
}

void ModelEditorTool::open_document_internal(const char* name)
{
	is_open = true;
}

void ModelEditorTool::close_internal()
{
	is_open = false;
}

bool ModelEditorTool::save_document_internal()
{
	return false;
}
