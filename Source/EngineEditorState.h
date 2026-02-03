#pragma once
#ifdef EDITOR_BUILD
#include "Framework/ConsoleCmdGroup.h"
#include <string>
#include <variant>
#include "Animation/Editor/Optional.h"
#include <vector>
#include <cassert>
#include <functional>
#include "Framework/MulticastDelegate.h"
#include "Framework/Range.h"

using std::variant;
using std::vector;
using std::string;
using std::function;
class IEditorTool;

class ClassTypeInfo;
class Texture;
struct View_Setup;
class EditorState {
public:
	EditorState();
	~EditorState();
	bool has_tool() { return curTool != nullptr; }
	IEditorTool* get_tool() { return curTool.get(); }
	// "closes" the current tool
	void hide();
	// "uncloses" current tool
	void unhide();
	bool wants_scene_viewport_menu_bar();
	void hook_pre_viewport();
	void hook_viewport();

	void open_tool(string mapname);

	const View_Setup* get_vs();

	void tick(float dt);
	void imgui_hook_new_frame();
	void imgui_draw();
	void draw_tab_window();
private:
	uptr<IEditorTool> curTool;
};
#endif