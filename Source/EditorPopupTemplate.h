#pragma once
#include "EditorPopups.h"
using std::string;	// sick of it
using std::vector;
using std::function;
class PopupTemplate
{
public:
	static void create_are_you_sure(
		EditorPopupManager* mgr,
		const string& desc,
		function<void()> continue_func 
	);
	static void create_basic_okay(
		EditorPopupManager* mgr,
		const std::string& title,
		const std::string& desc
	);

	static void create_file_save_as(
		EditorPopupManager* mgr,
		function<void(string)> on_save,
		string extension	// without dot
		);
};