#ifdef EDITOR_BUILD
#include "RoadBuilderTool.h"
#include "EditorDocLocal.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include <SDL3/SDL_scancode.h>

// ---- tick (main update) -----------------------------------------------------

void RoadBuilderTool::tick(EditorInputs& inputs)
{
    ASSERT(eng);

    // Redraw 3D course debug lines every frame
    if (course_preview.is_built)
        course_preview.debug_draw();

    if (!UiSystem::inst->is_vp_hovered()) return;

    auto* net = get_or_create_component();
    draw_overlay(net);

    if (!net) return;

    const glm::ivec2 mouse = Input::get_mouse_pos();
    const bool lmb_pressed  = Input::was_mouse_pressed(0);
    const bool lmb_down     = Input::is_mouse_down(0);
    const bool lmb_released = Input::was_mouse_released(0);
    const bool rmb_pressed  = Input::was_mouse_pressed(1);

    // ---- Keyboard shortcuts ----
    if (inputs.can_use_keyboard()) {
        if (Input::was_key_pressed(SDL_SCANCODE_ESCAPE)) {
            sub_mode = SubMode::PlaceNode;
            connect_src_id = -1;
            dragging = false;
            drag_ctrl_edge_id = -1;
        }
        if (Input::was_key_pressed(SDL_SCANCODE_C)) {
            sub_mode = (sub_mode == SubMode::Connect) ? SubMode::PlaceNode : SubMode::Connect;
            connect_src_id = -1;
        }
        if (Input::was_key_pressed(SDL_SCANCODE_X)) {
            sub_mode = (sub_mode == SubMode::Delete) ? SubMode::PlaceNode : SubMode::Delete;
        }
        if (Input::was_key_pressed(SDL_SCANCODE_DELETE)) {
            if (hovered_node >= 0) {
                net->remove_node(hovered_node);
                net->rebuild_mesh();
            } else if (hovered_edge >= 0) {
                net->remove_edge(hovered_edge);
                net->rebuild_mesh();
            }
        }
    }

    // Cancel with right-click
    if (rmb_pressed && inputs.can_use_mouse_click()) {
        sub_mode = SubMode::PlaceNode;
        connect_src_id = -1;
        dragging = false;
        inputs.eat_mouse_click();
        return;
    }

    // ---- Mode dispatch ----
    switch (sub_mode) {

    // -----------------------------------------------------------------------
    case SubMode::PlaceNode:
    {
        // -- Drag control point (highest priority) --
        if (lmb_pressed && inputs.can_use_mouse_click() && drag_ctrl_edge_id < 0) {
            int cedge; bool cis_a;
            if (pick_ctrl_pt(mouse, cedge, cis_a)) {
                drag_ctrl_edge_id = cedge;
                drag_ctrl_is_a    = cis_a;
                inputs.eat_mouse_click();
            }
        }

        if (drag_ctrl_edge_id >= 0) {
            glm::vec3 new_pos;
            if (ray_to_ground(mouse.x, mouse.y, new_pos)) {
                if (auto* edge = net->find_edge(drag_ctrl_edge_id)) {
                    glm::vec3 ca = edge->ctrl_a;
                    glm::vec3 cb = edge->ctrl_b;
                    if (drag_ctrl_is_a) ca = apply_snap(new_pos);
                    else                cb = apply_snap(new_pos);
                    net->set_edge_control_points(drag_ctrl_edge_id, ca, cb);
                    net->rebuild_mesh();
                }
            }
            if (lmb_released) drag_ctrl_edge_id = -1;
            return;
        }

        // -- Drag existing node --
        if (lmb_pressed && inputs.can_use_mouse_click()) {
            if (hovered_node >= 0) {
                dragging = true;
                drag_node_id = hovered_node;
                inputs.eat_mouse_click();
            }
        }

        if (dragging && drag_node_id >= 0) {
            glm::vec3 new_pos;
            if (ray_to_ground(mouse.x, mouse.y, new_pos)) {
                net->move_node(drag_node_id, apply_snap(new_pos));
                net->rebuild_mesh();
            }
            if (lmb_released) {
                dragging = false;
                drag_node_id = -1;
            }
            return;
        }

        // -- Place new node on click --
        if (lmb_pressed && inputs.can_use_mouse_click() && hovered_node < 0) {
            glm::vec3 hit_pos;
            if (ray_to_ground(mouse.x, mouse.y, hit_pos)) {
                net->add_node(apply_snap(hit_pos));
                net->rebuild_mesh();
            }
            inputs.eat_mouse_click();
        }
        break;
    }

    // -----------------------------------------------------------------------
    case SubMode::Connect:
    {
        if (lmb_pressed && inputs.can_use_mouse_click()) {
            if (connect_src_id < 0) {
                // First click: pick source node (or place one if empty)
                if (hovered_node >= 0) {
                    connect_src_id = hovered_node;
                } else {
                    glm::vec3 hit_pos;
                    if (ray_to_ground(mouse.x, mouse.y, hit_pos)) {
                        int new_id = net->add_node(apply_snap(hit_pos));
                        connect_src_id = new_id;
                        net->rebuild_mesh();
                    }
                }
            } else {
                // Second click: pick dest node (or place one), then connect
                int dst_id = hovered_node;
                if (dst_id < 0) {
                    glm::vec3 hit_pos;
                    if (ray_to_ground(mouse.x, mouse.y, hit_pos)) {
                        dst_id = net->add_node(apply_snap(hit_pos));
                        net->rebuild_mesh();
                    }
                }
                if (dst_id >= 0 && dst_id != connect_src_id) {
                    int eid = net->add_edge(connect_src_id, dst_id, road_width, curved);
                    if (eid >= 0) {
                        net->rebuild_mesh();
                        // Chain: make dst the new source for continuous road drawing
                        connect_src_id = dst_id;
                    }
                }
            }
            inputs.eat_mouse_click();
        }
        break;
    }

    // -----------------------------------------------------------------------
    case SubMode::Delete:
    {
        if (lmb_pressed && inputs.can_use_mouse_click()) {
            bool did_delete = false;
            if (hovered_node >= 0) {
                net->remove_node(hovered_node);
                did_delete = true;
            } else if (hovered_edge >= 0) {
                net->remove_edge(hovered_edge);
                did_delete = true;
            }
            if (did_delete) {
                net->rebuild_mesh();
            }
            inputs.eat_mouse_click();
        }
        break;
    }

    // -----------------------------------------------------------------------
    case SubMode::CourseRoute:
    {
        if (lmb_pressed && inputs.can_use_mouse_click()) {
            glm::vec3 hit_pos;
            if (ray_to_ground(mouse.x, mouse.y, hit_pos)) {
                route_hints.push_back(apply_snap(hit_pos));
            }
            inputs.eat_mouse_click();
        }
        break;
    }

    } // switch
}

#endif // EDITOR_BUILD
