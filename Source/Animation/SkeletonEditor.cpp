#ifdef EDITOR_BUILD
#include "SkeletonEditor.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Render/Model.h"
#include "Render/DrawPublic.h"
#include "Assets/AssetDatabase.h"
#include "Render/RenderObj.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/SelectionState.h"
#include <imgui.h>
#include <functional>
#include <vector>

SkeletonEditor::SkeletonEditor() = default;

SkeletonEditor::~SkeletonEditor() {
    if (mb_handle_.is_valid())
        idraw->get_scene()->remove_meshbuilder(mb_handle_);
}

void SkeletonEditor::set_asset(const std::string& cmdl_path) {
    if (cmdl_path == cmdl_path_)
        return;
    cmdl_path_ = cmdl_path;
    model_ = g_assets.find_sync_sptr<Model>(cmdl_path);
    selected_bone_ = -1;
}

// Append a classic "bone" pyramid (octahedral) from joint `from` to joint `to`.
static void push_bone_pyramid(MeshBuilder& mb, glm::vec3 from, glm::vec3 to, Color32 color) {
    glm::vec3 dir = to - from;
    float len = glm::length(dir);
    if (len < 1e-5f)
        return;
    dir /= len;

    glm::vec3 up = (glm::abs(dir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    up = glm::normalize(glm::cross(right, dir));

    float w = len * 0.1f;
    glm::vec3 base = from + dir * (len * 0.2f);
    glm::vec3 corner[4] = {
        base + right * w,
        base + up * w,
        base - right * w,
        base - up * w,
    };
    for (int i = 0; i < 4; i++) {
        glm::vec3 next = corner[(i + 1) % 4];
        mb.PushLine(corner[i], next, color); // square base
        mb.PushLine(corner[i], to, color);   // base -> child end (apex)
        mb.PushLine(corner[i], from, color);  // base -> parent end (root)
    }
}

void SkeletonEditor::update_debug_viz() {
    mb_.Begin();
    bool visible = false;

    do {
        if (!model_ || !model_->get_skel() || selected_bone_ < 0)
            break;

        auto* editor_doc = static_cast<EditorDoc*>(eng->get_tool());
        if (!editor_doc || !editor_doc->selection_state)
            break;
        if (!editor_doc->selection_state->has_only_one_selected())
            break;
        Entity* sel = editor_doc->selection_state->get_only_one_selected().get();
        if (!sel)
            break;

        auto* mesh = sel->get_component<MeshComponent>();
        if (!mesh || mesh->get_model() != model_.get())
            break;

        const MSkeleton* skel = model_->get_skel();
        const int num_bones = skel->get_num_bones();
        if (selected_bone_ >= num_bones)
            break;

        const glm::mat4 ws = sel->get_ws_transform();
        const auto& bones = skel->get_all_bones();
        AnimatorObject* anim = mesh->get_animator();
        std::vector<glm::mat4> bonemats;
        if (anim)
            bonemats = anim->get_global_bonemats();

        // Returns the joint origin in world space (animated pose if available, else bind pose).
        auto joint_ws = [&](int i) -> glm::vec3 {
            glm::vec3 model_pos = (i < (int)bonemats.size())
                ? glm::vec3(bonemats[i][3])
                : glm::vec3(bones[i].posematrix[3]);
            return glm::vec3(ws * glm::vec4(model_pos, 1.f));
        };

        const int b = selected_bone_;
        glm::vec3 joint = joint_ws(b);

        // Sphere radius scaled off the longest connected bone so it reads at any scale.
        float ref_len = 0.f;
        const int parent = skel->get_bone_parent(b);
        if (parent >= 0 && parent < num_bones)
            ref_len = glm::length(joint_ws(parent) - joint);
        for (int c = 0; c < num_bones; c++) {
            if (skel->get_bone_parent(c) == b)
                ref_len = std::max(ref_len, glm::length(joint_ws(c) - joint));
        }
        float radius = (ref_len > 1e-4f) ? ref_len * 0.08f : 0.02f;
        mb_.AddSphere(joint, radius, 8, 6, COLOR_GREEN);

        // Green pyramids out to each child joint.
        for (int c = 0; c < num_bones; c++) {
            if (skel->get_bone_parent(c) == b)
                push_bone_pyramid(mb_, joint, joint_ws(c), COLOR_GREEN);
        }
        // Orange pyramid back to the parent joint.
        if (parent >= 0 && parent < num_bones)
            push_bone_pyramid(mb_, joint, joint_ws(parent), Color32(255, 128, 0, 255));

        visible = true;
    } while (false);

    mb_.End();

    if (!mb_handle_.is_valid())
        mb_handle_ = idraw->get_scene()->register_meshbuilder();
    MeshBuilder_Object obj;
    obj.meshbuilder = &mb_;
    obj.transform = glm::mat4(1.f);
    obj.owner = nullptr;
    obj.visible = visible;
    obj.depth_tested = false; // draw on top, no depth test
    obj.use_background_color = true;
    idraw->get_scene()->update_meshbuilder(mb_handle_, obj);
}

void SkeletonEditor::draw_bone_tree() {
    const MSkeleton* skel = model_->get_skel();
    const int n = skel->get_num_bones();
    const auto& bones = skel->get_all_bones();

    // Build child lists from the flat parent-indexed bone array.
    std::vector<std::vector<int>> children(n);
    std::vector<int> roots;
    for (int i = 0; i < n; i++) {
        int p = skel->get_bone_parent(i);
        if (p >= 0 && p < n && p != i)
            children[p].push_back(i);
        else
            roots.push_back(i);
    }

    std::function<void(int)> draw_node = [&](int idx) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children[idx].empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (idx == selected_bone_)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx((void*)(intptr_t)idx, flags, "%s", bones[idx].strname.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            selected_bone_ = idx;
        if (open) {
            for (int c : children[idx])
                draw_node(c);
            ImGui::TreePop();
        }
    };

    for (int r : roots)
        draw_node(r);
}

void SkeletonEditor::imgui_draw() {
    // Optional section: render nothing for static (skeleton-less) models.
    if (!model_ || !model_->get_skel())
        return;

    ImGui::SeparatorText("Skeleton");
    ImGui::Text("%d bones", model_->get_skel()->get_num_bones());
    if (selected_bone_ >= 0 && selected_bone_ < model_->get_skel()->get_num_bones()) {
        ImGui::SameLine();
        ImGui::TextDisabled("|  selected: %s",
                            model_->get_skel()->get_all_bones()[selected_bone_].strname.c_str());
    }

    ImGui::BeginChild("##bonetree", ImVec2(0, 240), true);
    draw_bone_tree();
    ImGui::EndChild();

    update_debug_viz();
}

#endif // EDITOR_BUILD
