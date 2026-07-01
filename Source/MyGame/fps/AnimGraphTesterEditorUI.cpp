#include "AnimGraphTesterEditorUI.h"
#ifdef EDITOR_BUILD

#include "AnimGraphTester.h"
#include "imgui.h"
#include "Framework/MyImguiLib.h"

// Matches the corner layout built in AnimGraphTester::rebuild_graph()'s BlendSpace2D case.
static const std::vector<ImVec2> k_bs2d_verts = {ImVec2(-1, -1), ImVec2(1, -1), ImVec2(-1, 1), ImVec2(1, 1)};
static const std::vector<int> k_bs2d_indices = {0, 1, 2, 1, 3, 2};
static const std::vector<const char*> k_bs2d_names = {"clip0 (-1,-1)", "clip1 (1,-1)", "clip2 (-1,1)", "clip3 (1,1)"};

void AnimGraphTesterEditorUi::draw_blendspace_canvas() {
    ImVec2 pos(comp->bs2d_x, comp->bs2d_y);
    const bool changed =
        MyImDrawBlendSpace("##bs2d_canvas", k_bs2d_verts, k_bs2d_indices, k_bs2d_names, ImVec2(-1.2f, -1.2f),
                            ImVec2(1.2f, 1.2f), &pos);
    if (changed) {
        comp->bs2d_x = pos.x;
        comp->bs2d_y = pos.y;
        comp->bs2d_manual = true; // dragging the marker takes over from the auto-sweep
    }
}

bool AnimGraphTesterEditorUi::draw() {
    if (comp->mode != AnimGraphTestMode::BlendSpace2D)
        return false;

    ImGui::Separator();
    ImGui::TextUnformatted("Blend Space 2D Preview");
    ImGui::Checkbox("Manual Control##bs2d", &comp->bs2d_manual);
    ImGui::SameLine();
    if (ImGui::Button("Pop Out##bs2d"))
        show_blendspace_popup = true;

    draw_blendspace_canvas();

    if (show_blendspace_popup) {
        ImGui::SetNextWindowSize(ImVec2(480, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Blend Space 2D Preview##bs2d_popup", &show_blendspace_popup)) {
            ImGui::Checkbox("Manual Control##bs2d_popup", &comp->bs2d_manual);
            draw_blendspace_canvas();
        }
        ImGui::End();
    }

    return false;
}

#endif
