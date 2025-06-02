#include "EditorPopupTemplate.h"
#include "imgui.h"
void PopupTemplate::create_are_you_sure(EditorPopupManager* mgr, const string& desc, function<void()> continue_func)
{
	mgr->add_popup(
		"Are you sure?",
		[desc,continue_func]()->bool {
			ImGui::Text("%s",desc.c_str());
			ImGui::Spacing();
			bool ret = false;
			if (ImGui::Button("Continue")) {
				ret = true;
				continue_func();
			}
			ImGui::SameLine(0,20);
			if (ImGui::Button("Cancel")) {
				ret = true;
			}
			return ret;
		});
}

void PopupTemplate::create_basic_okay(EditorPopupManager* mgr, const std::string& title, const std::string& desc)
{
	mgr->add_popup(
		title,
		[desc]()->bool {
			ImGui::Text("%s", desc.c_str());
			ImGui::Spacing();
			return ImGui::Button("Okay");
		}
	);
}
