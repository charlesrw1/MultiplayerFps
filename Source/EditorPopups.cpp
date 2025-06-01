#include "EditorPopups.h"
#include "imgui.h"
#include "Framework/Util.h"
EditorPopupManager* EditorPopupManager::inst = nullptr;
void EditorPopupManager::draw_popup_index(int index)
{
    assert(index >= 0 && index < popup_stack.size());
    auto& popup = popup_stack.at(index);
    if (!popup.has_opened()) {
        popup.id = ++id_gen;
        ImGui::OpenPopup(popup.name.c_str());
    }
    const float ofs = index * 20;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    center.x += ofs;
    center.y += ofs;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(200, 100));

    if (ImGui::BeginPopupModal(popup.name.c_str())) {

        const bool wants_close = popup.callback();

        if (index < popup_stack.size() - 1)// recurse
            draw_popup_index(index + 1);

        if (wants_close) {
            ImGui::CloseCurrentPopup();
            assert(index == popup_stack.size()-1);
            popup_stack.pop_back();
        }
        ImGui::EndPopup();
    }
}

void EditorPopupManager::draw_popups()
{
    if (popup_stack.empty())
        return;
    draw_popup_index(0);
}

void EditorPopupManager::add_popup(const std::string& name, std::function<bool()> callback)
{
	Popup p;
	p.callback = std::move(callback);
    p.name = name;
	popup_stack.push_back(p);
}

bool EditorPopupManager::has_popup_open() const
{
	return popup_stack.empty();
}

bool EditorPopupManager::Popup::has_opened() const {
    return id != -1;
}
