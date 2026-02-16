#ifdef EDITOR_BUILD
#include "EngineEditorState.h"
#include "IEditorTool.h"
#include "Framework/Util.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "Framework/Config.h"
#include "Game/LevelAssets.h"
#include "Framework/MyImguiLib.h"
EditorState::EditorState(){
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
	if (curTool.get()) 
		return curTool->wants_scene_viewport_menu_bar();
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
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"

void EditorState::open_tool(string mapname)
{


	const bool good = eng->load_level(mapname);
	if (good) {
		uptr<EditorDoc> editorDoc(EditorDoc::create_scene(mapname));
		if (curTool)
			sys_print(Debug, "EditorState::open_tool: replacing current tool\n");
		this->curTool = std::move(editorDoc);
	}
	else {
		//sys_print(Warning, "CreateLevelEditorAync::execute: failed to load map (%s)\n", assetPath.value_or("<unnamed>").c_str());
		sys_print(Warning, "EditorState::open_tool: creation returned null\n");
		this->curTool = nullptr;
	}

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
	else {
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	}
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

void EditorState::draw_tab_window()
{
	
}
#endif