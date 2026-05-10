#ifdef EDITOR_BUILD
#include "RoadBuilderTool.h"
#include "EditorDocLocal.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Game/Components/SpawnerComponenth.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Level.h"
#include "Game/BikeDemo/BikeCourse.h"
#include <string>
#include <vector>

// ---- ImGui panel ------------------------------------------------------------

void RoadBuilderTool::draw_ui()
{
    ASSERT(eng);
    if (ImGui::Begin("Road Builder")) {
        ImGui::TextDisabled("Left-click: place node | C: connect | X: delete | ESC: cancel");
        ImGui::Separator();

        // Sub-mode buttons
        bool in_place   = sub_mode == SubMode::PlaceNode;
        bool in_connect = sub_mode == SubMode::Connect;
        bool in_delete  = sub_mode == SubMode::Delete;

        if (in_place)   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.f));
        if (ImGui::Button("Place (default)")) { sub_mode = SubMode::PlaceNode; connect_src_id = -1; }
        if (in_place)   ImGui::PopStyleColor();

        ImGui::SameLine();

        if (in_connect) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.f));
        if (ImGui::Button("Connect [C]")) {
            sub_mode = (sub_mode == SubMode::Connect) ? SubMode::PlaceNode : SubMode::Connect;
            connect_src_id = -1;
        }
        if (in_connect) ImGui::PopStyleColor();

        ImGui::SameLine();

        if (in_delete)  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.f));
        if (ImGui::Button("Delete [X]")) {
            sub_mode = (sub_mode == SubMode::Delete) ? SubMode::PlaceNode : SubMode::Delete;
        }
        if (in_delete)  ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Checkbox("Curved roads", &curved);
        ImGui::SliderFloat("Road width", &road_width, 2.f, 20.f, "%.1f");

        if (sub_mode == SubMode::Connect && connect_src_id >= 0)
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Click destination node (or empty to place one)");

        ImGui::Separator();

        // ---- Course Preview ----
        if (ImGui::CollapsingHeader("Course Preview")) {
            ImGui::TextDisabled("Place ordered route hints, then Build Course.");
            ImGui::Spacing();

            // Sub-mode button
            bool in_route = (sub_mode == SubMode::CourseRoute);
            if (in_route) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.8f, 1.f));
            if (ImGui::Button("Place Route Hints")) {
                sub_mode = in_route ? SubMode::PlaceNode : SubMode::CourseRoute;
            }
            if (in_route) ImGui::PopStyleColor();
            if (in_route) ImGui::SameLine(), ImGui::TextColored(ImVec4(1.f, 0.4f, 1.f, 1.f), "Click viewport to add");

            ImGui::Spacing();

            // Route hints list
            if (route_hints.empty()) {
                ImGui::TextDisabled("No route hints placed.");
            } else {
                for (int i = 0; i < (int)route_hints.size(); ++i) {
                    ImGui::PushID(i);
                    ImGui::TextColored(ImVec4(1.f, 0.4f, 1.f, 1.f), "[%d]", i);
                    ImGui::SameLine();
                    ImGui::Text("(%.1f, %.1f, %.1f)", route_hints[i].x, route_hints[i].y, route_hints[i].z);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        route_hints.erase(route_hints.begin() + i);
                        --i;
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Spacing();
            ImGui::Checkbox("Loop", &course_loop);
            ImGui::SliderFloat("Sample step (m)", &sample_step, 0.2f, 5.f, "%.2f");

            ImGui::Separator();
            ImGui::TextDisabled("Racing line (hinge-spring physics)");

            bool rl_dirty = false;
            rl_dirty |= ImGui::DragFloat("RL k (stiffness)", &rl_k, 0.01f);
            rl_dirty |= ImGui::DragFloat("RL mass",          &rl_mass, 0.1f);
            rl_dirty |= ImGui::DragInt("RL iterations",      &rl_iters, 10, 1, 5000);
            if (rl_dirty && course_preview.is_built) {
                BikeCourse::compute_racing_line(course_preview.waypoints,
                                                course_preview.is_loop, rl_k, rl_mass, rl_dt, rl_iters);
            }

            ImGui::Checkbox("Show racing line overlay", &show_racing_line);
            ImGui::Checkbox("Show road width ticks",   &show_road_widths);

            ImGui::Separator();

            // Find without creating — don't spawn a component just because the panel is open
            auto* find_net = [this]() -> RoadNetworkComponent* {
                auto* level = eng->get_level();
                if (!level) return nullptr;
                auto* c = level->find_first_component(&RoadNetworkComponent::StaticType);
                return static_cast<RoadNetworkComponent*>(c);
            }();

            const bool has_net   = (find_net != nullptr);
            const bool has_hints = (route_hints.size() >= 2);

            if (!has_net)   ImGui::TextColored(ImVec4(1.f, 0.5f, 0.2f, 1.f), "No road network in scene");
            if (!has_hints) ImGui::TextColored(ImVec4(1.f, 0.5f, 0.2f, 1.f), "Need >= 2 route hints");

            ImGui::BeginDisabled(!has_net || !has_hints);
            if (ImGui::Button("Build Course")) {
                course_preview = BikeCourse{};
                course_preview.build_from_road_network(*find_net, route_hints, sample_step, course_loop);
                if (course_preview.is_built) {
                    BikeCourse::compute_racing_line(course_preview.waypoints,
                                                    course_preview.is_loop, rl_k, rl_mass, rl_dt, rl_iters);
                } else {
                    ImGui::SetNextWindowSize({ 300.f, 0.f });
                    ImGui::OpenPopup("build_failed");
                }
            }
            ImGui::EndDisabled();
            if (ImGui::BeginPopupModal("build_failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Course build failed — check road network\nconnectivity and route hint positions.");
                if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::BeginDisabled(!course_preview.is_built);
            if (ImGui::Button("Rebuild RL")) {
                BikeCourse::compute_racing_line(course_preview.waypoints,
                                                course_preview.is_loop, rl_k, rl_mass, rl_dt, rl_iters);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                route_hints.clear();
                course_preview = BikeCourse{};
            }

            ImGui::Spacing();
            ImGui::BeginDisabled(!has_hints);
            if (ImGui::Button("Bake Waypoints to Level")) {
                // Remove all existing bike_waypoint spawner entities
                std::vector<Entity*> to_remove;
                for (auto* obj : eng->get_level()->get_all_objects()) {
                    if (auto* s = obj->cast_to<SpawnerComponent>()) {
                        if (s->get_spawner_type() == "bike_waypoint")
                            to_remove.push_back(s->get_owner());
                    }
                }
                for (auto* e : to_remove)
                    doc.remove_scene_object(e);

                // Spawn one bike_waypoint entity per route hint, named by index
                for (int i = 0; i < (int)route_hints.size(); ++i) {
                    Entity* e = doc.spawn_entity();
                    e->set_editor_name(std::to_string(i));
                    auto* sc = static_cast<SpawnerComponent*>(
                        doc.attach_component(&SpawnerComponent::StaticType, e));
                    sc->set("bike_waypoint");
                    e->set_ws_position(route_hints[i]);
                }
                sys_print(Info, "RoadBuilder: baked %d bike_waypoint spawners (removed %d old)\n",
                          (int)route_hints.size(), (int)to_remove.size());
            }
            ImGui::EndDisabled();

            if (course_preview.is_built) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                    "%.0f m  |  %d waypoints%s",
                    course_preview.total_length_m,
                    (int)course_preview.waypoints.size(),
                    course_preview.is_loop ? "  (loop)" : "");

                course_preview.debug_draw_fillets();
            }
        }
    }
    ImGui::End();
}

#endif // EDITOR_BUILD
