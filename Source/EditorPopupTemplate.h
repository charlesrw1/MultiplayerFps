#pragma once
#include "EditorPopups.h"
using std::function;
using std::string; // sick of it
using std::vector;
class IEditorTool;
class PopupTemplate
{
public:
	static void create_are_you_sure(EditorPopupManager* mgr, const string& desc, function<void()> continue_func);
	static void create_basic_okay(EditorPopupManager* mgr, const std::string& title, const std::string& desc);

	static void create_file_save_as(EditorPopupManager* mgr, function<void(string)> on_save,
									string extension // without dot
	);

	// Prompts to save/discard/cancel before continuing with an action that would discard
	// unsaved changes on "tool" (eg. switching to a different scene/prefab). "continue_func"
	// is only invoked once the document is no longer dirty (saved) or the user chose Discard.
	static void create_unsaved_changes_prompt(EditorPopupManager* mgr, IEditorTool* tool,
											  function<void()> continue_func);
};