#include "EngineEditorState.h"
#include "IEditorTool.h"
#include "Framework/Util.h"
#include "imgui.h"
EditorState::EditorState()
{
}

EditorState::~EditorState()
{
}

void EditorState::hide()
{
	if (curTool) {
		sys_print(Info, "EditorState: Closing tool\n");
		curTool.reset();
	}

}

bool EditorState::wants_scene_viewport_menu_bar() {
	if (curTool.get()) return curTool->wants_scene_viewport_menu_bar();
	return false;
}

void EditorState::hook_pre_viewport()
{
	if (curTool)
		curTool->hook_pre_scene_viewport_draw();
}
void EditorState::hook_viewport()
{
	if (curTool)
		curTool->hook_scene_viewport_draw();
}

void EditorState::open_tool(uptr<CreateEditorAsync> creation, bool set_active, function<void(bool)> callback)
{
	assert(creation);
	sys_print(Debug, "EditorState::open_tool: calling creator\n");
	creation->execute([this, callback](uptr<IEditorTool> arg) {
		if (!arg)
			sys_print(Warning, "EditorState::open_tool: creation returned null\n");

		if (curTool&&arg) {
			sys_print(Debug, "EditorState::open_tool: replacing current tool\n");
		}
		this->curTool = std::move(arg);
		callback(this->curTool != nullptr);
		});
}

const View_Setup* EditorState::get_vs()
{
	if (curTool)
		return curTool->get_vs();
	return nullptr;
}

void EditorState::tick(float dt)
{
	if (curTool)
		curTool->tick(dt);
}

void EditorState::imgui_hook_new_frame()
{
	if (curTool)
		curTool->hook_imgui_newframe();
}

void EditorState::imgui_draw()
{
	if (curTool)
		curTool->draw_imgui_public();
}
