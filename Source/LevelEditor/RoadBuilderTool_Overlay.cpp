#ifdef EDITOR_BUILD
#include "RoadBuilderTool.h"
#include "EditorDocLocal.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include <vector>

// Screen-space radius for drawing node circles
static constexpr float NODE_DRAW_RADIUS = 8.f;

// ---- colour palette ---------------------------------------------------------

static constexpr ImU32 COL_NODE_DEFAULT  = IM_COL32(80,  160, 255, 220);
static constexpr ImU32 COL_NODE_HOVERED  = IM_COL32(255, 220,  40, 240);
static constexpr ImU32 COL_NODE_SELECTED = IM_COL32( 40, 220,  80, 240);
static constexpr ImU32 COL_EDGE_DEFAULT  = IM_COL32(120, 180, 255, 180);
static constexpr ImU32 COL_EDGE_HOVERED  = IM_COL32(255, 200,  40, 200);
static constexpr ImU32 COL_PREVIEW       = IM_COL32(200, 255, 100, 180);
static constexpr ImU32 COL_CTRL_PT      = IM_COL32(255, 120,  50, 200);
static constexpr ImU32 COL_HINT         = IM_COL32(255,  50, 200, 230);
static constexpr ImU32 COL_COURSE_CTR   = IM_COL32(100, 255, 100, 160);
static constexpr ImU32 COL_RACING_LINE  = IM_COL32(255, 153,   0, 230);
static constexpr ImU32 COL_ROAD_WIDTH   = IM_COL32(255, 255, 255,  80);

// ---- overlay drawing --------------------------------------------------------

void RoadBuilderTool::draw_overlay(const RoadNetworkComponent* net) const
{
    ASSERT(net);
    if (!net) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const glm::ivec2 mouse = Input::get_mouse_pos();

    // Recompute hovered IDs each frame (mutable); ctrl pts take precedence over edges
    hovered_node = pick_node(mouse);
    bool has_ctrl_hover = (sub_mode == SubMode::PlaceNode) &&
                          pick_ctrl_pt(mouse, hovered_ctrl_edge, hovered_ctrl_is_a);
    if (!has_ctrl_hover) hovered_ctrl_edge = -1;
    hovered_edge = (hovered_node < 0 && !has_ctrl_hover) ? pick_edge(mouse) : -1;

    // ---- Edges ----
    for (const auto& e : net->get_edges()) {
        const auto* na = net->find_node(e.node_a_id);
        const auto* nb = net->find_node(e.node_b_id);
        if (!na || !nb) continue;

        ImU32 col = (e.id == hovered_edge) ? COL_EDGE_HOVERED : COL_EDGE_DEFAULT;

        if (!e.curved) {
            ImVec2 sa = world_to_screen(na->position);
            ImVec2 sb = world_to_screen(nb->position);
            dl->AddLine(sa, sb, col, 2.f);
        } else {
            // Bezier polyline
            std::vector<ImVec2> pts;
            const int N = 20;
            for (int i = 0; i <= N; i++) {
                float t = float(i) / float(N);
                float u = 1.f - t;
                glm::vec3 p = u*u*u*na->position
                            + 3.f*u*u*t*e.ctrl_a
                            + 3.f*u*t*t*e.ctrl_b
                            + t*t*t*nb->position;
                pts.push_back(world_to_screen(p));
            }
            dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, 2.f);

            // Control point handles (editor-only, draggable dots)
            bool ca_hov  = (hovered_ctrl_edge == e.id &&  hovered_ctrl_is_a);
            bool cb_hov  = (hovered_ctrl_edge == e.id && !hovered_ctrl_is_a);
            bool ca_drag = (drag_ctrl_edge_id  == e.id &&  drag_ctrl_is_a);
            bool cb_drag = (drag_ctrl_edge_id  == e.id && !drag_ctrl_is_a);
            ImU32 col_ca = (ca_drag || ca_hov) ? IM_COL32(255,220,40,240) : COL_CTRL_PT;
            ImU32 col_cb = (cb_drag || cb_hov) ? IM_COL32(255,220,40,240) : COL_CTRL_PT;
            float r_ca = (ca_drag || ca_hov) ? 7.f : 4.f;
            float r_cb = (cb_drag || cb_hov) ? 7.f : 4.f;
            dl->AddCircleFilled(world_to_screen(e.ctrl_a), r_ca, col_ca);
            dl->AddCircleFilled(world_to_screen(e.ctrl_b), r_cb, col_cb);
            dl->AddLine(world_to_screen(na->position), world_to_screen(e.ctrl_a), COL_CTRL_PT, 1.f);
            dl->AddLine(world_to_screen(nb->position), world_to_screen(e.ctrl_b), COL_CTRL_PT, 1.f);
        }
    }

    // ---- Nodes ----
    for (const auto& n : net->get_nodes()) {
        ImVec2 s = world_to_screen(n.position);
        bool is_hover   = (n.id == hovered_node);
        bool is_src     = (n.id == connect_src_id);
        ImU32 fill = is_src     ? COL_NODE_SELECTED :
                     is_hover   ? COL_NODE_HOVERED  :
                                  COL_NODE_DEFAULT;
        dl->AddCircleFilled(s, NODE_DRAW_RADIUS, fill);
        dl->AddCircle(s, NODE_DRAW_RADIUS, IM_COL32(255,255,255,160), 0, 1.5f);
    }

    // ---- Connect preview line ----
    if (sub_mode == SubMode::Connect && connect_src_id >= 0) {
        const auto* src = net->find_node(connect_src_id);
        if (src) {
            glm::vec3 world_mouse;
            if (ray_to_ground(mouse.x, mouse.y, world_mouse)) {
                // Snap to hovered node if close
                if (hovered_node >= 0 && hovered_node != connect_src_id) {
                    if (auto* hn = net->find_node(hovered_node))
                        world_mouse = hn->position;
                }
                ImVec2 sa = world_to_screen(src->position);
                ImVec2 sb = world_to_screen(world_mouse);
                dl->AddLine(sa, sb, COL_PREVIEW, 2.f);
                if (curved) {
                    // Show the bezier preview
                    glm::vec3 delta = world_mouse - src->position;
                    glm::vec3 ca = src->position + delta * (1.f/3.f);
                    glm::vec3 cb = src->position + delta * (2.f/3.f);
                    // Reuse ctrl handle dots
                    dl->AddCircleFilled(world_to_screen(ca), 4.f, COL_CTRL_PT);
                    dl->AddCircleFilled(world_to_screen(cb), 4.f, COL_CTRL_PT);
                }
            }
        }
    }

    // ---- Place preview: ghost node under mouse ----
    if (sub_mode == SubMode::PlaceNode && hovered_node < 0 && !dragging) {
        glm::vec3 world_mouse;
        if (ray_to_ground(mouse.x, mouse.y, world_mouse)) {
            world_mouse = apply_snap(world_mouse);
            ImVec2 s = world_to_screen(world_mouse);
            dl->AddCircle(s, NODE_DRAW_RADIUS, COL_PREVIEW, 0, 1.5f);
        }
    }

    // ---- Course Preview: center line + racing line ----
    if (course_preview.is_built) {
        const auto& wps = course_preview.waypoints;
        const int   n   = (int)wps.size();
        const int   num_segs = course_preview.is_loop ? n : n - 1;

        for (int i = 0; i < num_segs; ++i) {
            const BikeWaypoint& wp   = wps[i];
            const BikeWaypoint& next = wps[(i + 1) % n];

            // Center line
            dl->AddLine(world_to_screen(wp.position), world_to_screen(next.position),
                        COL_COURSE_CTR, 1.5f);

            // Road-width tick every 10th waypoint
            if (show_road_widths && i % 10 == 0) {
                ImVec2 l = world_to_screen(wp.position - wp.right * wp.road_half_width);
                ImVec2 r = world_to_screen(wp.position + wp.right * wp.road_half_width);
                dl->AddLine(l, r, COL_ROAD_WIDTH, 1.f);
            }

            // Racing line — use pre-baked world positions, not lateral offsets
            if (show_racing_line) {
                dl->AddLine(world_to_screen(wp.racing_line_pos),
                            world_to_screen(next.racing_line_pos),
                            COL_RACING_LINE, 2.f);
            }
        }
    }

    // ---- Route hint markers ----
    for (int i = 0; i < (int)route_hints.size(); ++i) {
        ImVec2 s = world_to_screen(route_hints[i]);
        dl->AddCircleFilled(s, 8.f, COL_HINT);
        dl->AddCircle(s, 8.f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        char buf[8]; std::snprintf(buf, sizeof(buf), "%d", i);
        dl->AddText({ s.x + 10.f, s.y - 8.f }, IM_COL32(255, 255, 255, 255), buf);
    }

    // ---- Ghost hint in CourseRoute mode ----
    if (sub_mode == SubMode::CourseRoute) {
        glm::vec3 world_mouse;
        if (ray_to_ground(mouse.x, mouse.y, world_mouse)) {
            world_mouse = apply_snap(world_mouse);
            ImVec2 s = world_to_screen(world_mouse);
            dl->AddCircle(s, 8.f, COL_HINT, 0, 1.5f);
        }
    }
}

#endif // EDITOR_BUILD
