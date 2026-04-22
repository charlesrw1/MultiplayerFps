#ifdef EDITOR_BUILD
#include "RoadBuilderTool.h"
#include "EditorDocLocal.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "Physics/Physics2.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Level.h"
#include <cmath>
#include <algorithm>

// Screen-space radius for node hit-testing (pixels)
static constexpr float NODE_HIT_RADIUS   = 12.f;
// Screen-space radius for drawing node circles
static constexpr float NODE_DRAW_RADIUS  = 8.f;
// Pixel threshold for edge hit-testing
static constexpr float EDGE_HIT_DIST     = 10.f;

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

// ---- construction -----------------------------------------------------------

RoadBuilderTool::RoadBuilderTool(EditorDoc& doc) : doc(doc)
{
    mb_handle = idraw->get_scene()->register_meshbuilder();
}

RoadBuilderTool::~RoadBuilderTool()
{
    idraw->get_scene()->remove_meshbuilder(mb_handle);
}

// ---- helpers ----------------------------------------------------------------

RoadNetworkComponent* RoadBuilderTool::get_or_create_component()
{
    auto* level = eng->get_level();
    if (!level) return nullptr;

    auto* existing = level->find_first_component(&RoadNetworkComponent::StaticType);
    if (existing)
        return static_cast<RoadNetworkComponent*>(existing);

    // Create a new entity with a RoadNetworkComponent
    Entity* e = doc.spawn_entity();
    e->set_editor_name("RoadNetwork");
    auto* comp = static_cast<RoadNetworkComponent*>(
        doc.attach_component(&RoadNetworkComponent::StaticType, e));
    return comp;
}

ImVec2 RoadBuilderTool::world_to_screen(glm::vec3 world) const
{
    const View_Setup* vs = doc.get_vs();
    if (!vs) return { -99999.f, -99999.f };

    glm::vec4 clip = vs->viewproj * glm::vec4(world, 1.f);
    if (clip.w < 0.001f) return { -99999.f, -99999.f };

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    Rect2d vp = UiSystem::inst->get_vp_rect();

    float sx = (ndc.x * 0.5f + 0.5f) * float(vp.w) + float(vp.x);
    float sy = (1.f - (ndc.y * 0.5f + 0.5f)) * float(vp.h) + float(vp.y);
    return { sx, sy };
}

int RoadBuilderTool::pick_node(glm::ivec2 mouse_screen) const
{
    auto* level = eng->get_level();
    if (!level) return -1;
    auto* comp = static_cast<const RoadNetworkComponent*>(
        level->find_first_component(&RoadNetworkComponent::StaticType));
    if (!comp) return -1;

    float best_dist2 = NODE_HIT_RADIUS * NODE_HIT_RADIUS;
    int   best_id   = -1;

    for (const auto& n : comp->get_nodes()) {
        ImVec2 s = world_to_screen(n.position);
        float dx = s.x - float(mouse_screen.x);
        float dy = s.y - float(mouse_screen.y);
        float d2 = dx*dx + dy*dy;
        if (d2 < best_dist2) { best_dist2 = d2; best_id = n.id; }
    }
    return best_id;
}

int RoadBuilderTool::pick_edge(glm::ivec2 mouse_screen) const
{
    auto* level = eng->get_level();
    if (!level) return -1;
    auto* comp = static_cast<const RoadNetworkComponent*>(
        level->find_first_component(&RoadNetworkComponent::StaticType));
    if (!comp) return -1;

    float best_dist = EDGE_HIT_DIST;
    int   best_id  = -1;

    glm::vec2 mp = { float(mouse_screen.x), float(mouse_screen.y) };

    for (const auto& e : comp->get_edges()) {
        const auto* na = comp->find_node(e.node_a_id);
        const auto* nb = comp->find_node(e.node_b_id);
        if (!na || !nb) continue;

        // Sample edge line (straight or rough bezier mid) - check a few points
        auto sample_edge = [&](float t) -> glm::vec2 {
            glm::vec3 p;
            if (!e.curved) {
                p = glm::mix(na->position, nb->position, t);
            } else {
                float u = 1.f - t;
                p = u*u*u*na->position
                  + 3.f*u*u*t*e.ctrl_a
                  + 3.f*u*t*t*e.ctrl_b
                  + t*t*t*nb->position;
            }
            ImVec2 s = world_to_screen(p);
            return { s.x, s.y };
        };

        for (int seg = 0; seg < 8; seg++) {
            float t0 = float(seg)   / 8.f;
            float t1 = float(seg+1) / 8.f;
            glm::vec2 a = sample_edge(t0);
            glm::vec2 b = sample_edge(t1);

            // Point-to-segment distance
            glm::vec2 ab = b - a;
            float len2 = glm::dot(ab, ab);
            float dist;
            if (len2 < 0.001f) {
                dist = glm::length(mp - a);
            } else {
                float t = glm::clamp(glm::dot(mp - a, ab) / len2, 0.f, 1.f);
                dist = glm::length(mp - (a + t * ab));
            }
            if (dist < best_dist) { best_dist = dist; best_id = e.id; }
        }
    }
    return best_id;
}

bool RoadBuilderTool::pick_ctrl_pt(glm::ivec2 mouse_screen, int& out_edge_id, bool& out_is_a) const
{
    auto* level = eng->get_level();
    if (!level) return false;
    auto* comp = static_cast<const RoadNetworkComponent*>(
        level->find_first_component(&RoadNetworkComponent::StaticType));
    if (!comp) return false;

    const float HIT_R = 10.f;
    float best = HIT_R * HIT_R;
    bool found = false;

    for (const auto& e : comp->get_edges()) {
        if (!e.curved) continue;
        for (int k = 0; k < 2; k++) {
            glm::vec3 wp = (k == 0) ? e.ctrl_a : e.ctrl_b;
            ImVec2 s = world_to_screen(wp);
            float dx = s.x - float(mouse_screen.x);
            float dy = s.y - float(mouse_screen.y);
            float d2 = dx*dx + dy*dy;
            if (d2 < best) {
                best = d2;
                out_edge_id = e.id;
                out_is_a    = (k == 0);
                found = true;
            }
        }
    }
    return found;
}

bool RoadBuilderTool::ray_to_ground(int mx, int my, glm::vec3& out) const
{
    // First try physics raycast to find actual ground
    glm::vec3 dir = doc.unproject_mouse_to_ray(mx, my).dir;
    glm::vec3 pos = doc.get_vs()->origin;

    world_query_result res;
    if (g_physics.trace_ray(res, pos, pos - dir * 2000.f, nullptr, UINT32_MAX)) {
        out = res.hit_pos;
        return true;
    }
    return false;
}

glm::vec3 RoadBuilderTool::apply_snap(glm::vec3 pos) const
{
    if (!ed_has_snap.get_bool()) return pos;
    float snap = ed_translation_snap.get_float();
    if (snap < 0.001f) return pos;
    pos.x = std::round(pos.x / snap) * snap;
    pos.z = std::round(pos.z / snap) * snap;
    return pos;
}

// ---- overlay drawing --------------------------------------------------------

void RoadBuilderTool::draw_overlay(const RoadNetworkComponent* net) const
{
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
            bool ca_hov = (hovered_ctrl_edge == e.id &&  hovered_ctrl_is_a);
            bool cb_hov = (hovered_ctrl_edge == e.id && !hovered_ctrl_is_a);
            bool ca_drag = (drag_ctrl_edge_id == e.id &&  drag_ctrl_is_a);
            bool cb_drag = (drag_ctrl_edge_id == e.id && !drag_ctrl_is_a);
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

// ---- tick (main update) -----------------------------------------------------

void RoadBuilderTool::tick(EditorInputs& inputs)
{
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

// ---- ImGui panel ------------------------------------------------------------

void RoadBuilderTool::draw_ui()
{
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

            // Auto-rebuild racing line when strength changes
            if (ImGui::SliderFloat("RL strength", &rl_strength, 0.f, 1.f, "%.2f")
                    && course_preview.is_built) {
                BikeCourse::compute_racing_line(course_preview.waypoints,
                                                course_preview.is_loop, rl_strength);
            }

            static float sample_step = 0.5f;
            ImGui::SliderFloat("Sample step (m)", &sample_step, 0.2f, 5.f, "%.1f");

            ImGui::Checkbox("Show racing line overlay", &show_racing_line);
            ImGui::Checkbox("Show road width ticks", &show_road_widths);

            ImGui::Spacing();

            // Find without creating — don't spawn a component just because the panel is open
            auto* find_net = [this]() -> RoadNetworkComponent* {
                auto* level = eng->get_level();
                if (!level) return nullptr;
                auto* c = level->find_first_component(&RoadNetworkComponent::StaticType);
                return static_cast<RoadNetworkComponent*>(c);
            }();

            const bool has_net   = (find_net != nullptr);
            const bool has_hints = (route_hints.size() >= 2);

            if (!has_net)   ImGui::TextColored(ImVec4(1.f, 0.4f, 0.2f, 1.f), "No road network in scene");
            if (!has_hints) ImGui::TextColored(ImVec4(1.f, 0.4f, 0.2f, 1.f), "Need >= 2 route hints");

            ImGui::BeginDisabled(!has_net || !has_hints);
            if (ImGui::Button("Build Course")) {
                course_preview = BikeCourse{};
                course_preview.build_from_road_network(*find_net, route_hints, sample_step, course_loop);
                if (course_preview.is_built)
                    BikeCourse::compute_racing_line(course_preview.waypoints,
                                                    course_preview.is_loop, rl_strength);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!course_preview.is_built);
            if (ImGui::Button("Rebuild RL")) {
                BikeCourse::compute_racing_line(course_preview.waypoints,
                                                course_preview.is_loop, rl_strength);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Clear")) {
                route_hints.clear();
                course_preview = BikeCourse{};
            }

            if (course_preview.is_built) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                    "Course: %.0f m, %d wpts%s",
                    course_preview.total_length_m,
                    (int)course_preview.waypoints.size(),
                    course_preview.is_loop ? " (loop)" : "");
            }
        }
    }
    ImGui::End();
}

#endif // EDITOR_BUILD
