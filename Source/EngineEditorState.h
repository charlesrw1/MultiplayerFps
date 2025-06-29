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
	
	void select_tab(int index);

	MulticastDelegate<> on_change_tool;
	MulticastDelegate<> on_hide;
	MulticastDelegate<> on_unhide;
private:
	uptr<IEditorTool> curTool;

	// states:
	// open (tabs or no tabs)
	// closed. not being drawn (but maybe background tabs)

	// list of tabs
	struct ClosedTab {
		uptr<CreateEditorAsync> creationCmd;
	};
	struct OpenedTab {
		uptr<IEditorTool> tool = nullptr;
	};
	using Tab = variant<OpenedTab, ClosedTab>;

	void validate() {
		assert(!last_tab.has_value() || !(last_tab.value() >= tabs.size() || last_tab.value() < 0));
		struct VisitTab {
			opt<bool> operator()(const OpenedTab& val) const { 
				if (!val.tool) return std::nullopt;
				return true; 
			}
			opt<bool> operator()(const ClosedTab& ex) const { return false; }
		};
		bool found_open = false;
		for (auto& tab : tabs) {
			const opt<bool> open = std::visit(VisitTab{}, tab);
			assert(open.has_value());
			assert(!(*open) || !found_open);
			found_open = true;
		}
	}
	opt<int> last_tab;
	vector<Tab> tabs;
};
