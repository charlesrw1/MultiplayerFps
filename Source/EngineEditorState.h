#pragma once

#include "Framework/ConsoleCmdGroup.h"
#include <string>
#include <variant>
#include "Animation/Editor/Optional.h"
#include <vector>
#include <cassert>
#include <functional>
#include "Framework/MulticastDelegate.h"

using std::variant;
using std::vector;
using std::string;
using std::function;
class IEditorTool;
class CreateEditorAsync {
public:
	using Callback = function<void(uptr<IEditorTool>)>;
	virtual ~CreateEditorAsync() {}
	virtual void execute(Callback callback) = 0;
	virtual string get_tab_name() = 0;
	virtual opt<string> get_asset_name() = 0;
};
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

	void open_tool(uptr<CreateEditorAsync> creation/* non null*/, bool set_active, function<void(bool)> callback);

	const View_Setup* get_vs();

	void tick(float dt);
	void imgui_hook_new_frame();
	void imgui_draw();
	void draw_tab_window();
	
	void select_tab(int index);

	MulticastDelegate<> on_change_tool;
	MulticastDelegate<> on_hide;
	MulticastDelegate<> on_unhide;
private:
	uptr<IEditorTool> curTool;
	void set_tab_open(string name) {
		bool found = false;
		for (auto& t : tabs) {
			if (t.assetName == name) {
				found = true;
				t.is_active = true;
			}
			else {
				t.is_active = false;
			}
		}
		if (!found) {
			tabs.push_back({});
			tabs.back().assetName = name;
			tabs.back().is_active = true;
		}
	}
	struct TabItem {
		string itemName;
		const ClassTypeInfo* assetType = nullptr;
		string assetName;
		bool is_active = false;
	};
	vector<TabItem> tabs;
	const Texture* redCrossIcon = nullptr;
};
