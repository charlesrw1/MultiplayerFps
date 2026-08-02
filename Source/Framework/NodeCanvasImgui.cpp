#include "NodeCanvasImgui.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include <cmath>
#include <algorithm>

namespace {
ImVec2 bezier_point(ImVec2 p0, ImVec2 c0, ImVec2 c1, ImVec2 p1, float t) {
    float u = 1.f - t;
    float w0 = u * u * u, w1 = 3.f * u * u * t, w2 = 3.f * u * t * t, w3 = t * t * t;
    return ImVec2(p0.x * w0 + c0.x * w1 + c1.x * w2 + p1.x * w3,
                  p0.y * w0 + c0.y * w1 + c1.y * w2 + p1.y * w3);
}
void link_control_points(ImVec2 p0, ImVec2 p1, ImVec2& c0, ImVec2& c1) {
    float d = std::max(std::fabs(p1.x - p0.x) * 0.5f, 40.f);
    c0 = ImVec2(p0.x + d, p0.y);
    c1 = ImVec2(p1.x - d, p1.y);
}
float distance_to_link(ImVec2 p0, ImVec2 p1, ImVec2 mouse) {
    ImVec2 c0, c1;
    link_control_points(p0, p1, c0, c1);
    float best = 1e9f;
    const int steps = 16;
    ImVec2 prev = p0;
    for (int i = 1; i <= steps; i++) {
        ImVec2 cur = bezier_point(p0, c0, c1, p1, (float)i / steps);
        // distance from mouse to segment prev-cur
        ImVec2 seg = ImVec2(cur.x - prev.x, cur.y - prev.y);
        float len2 = seg.x * seg.x + seg.y * seg.y;
        float t = len2 > 0.f ? std::clamp(((mouse.x - prev.x) * seg.x + (mouse.y - prev.y) * seg.y) / len2, 0.f, 1.f) : 0.f;
        ImVec2 closest(prev.x + seg.x * t, prev.y + seg.y * t);
        float dx = mouse.x - closest.x, dy = mouse.y - closest.y;
        best = std::min(best, dx * dx + dy * dy);
        prev = cur;
    }
    return std::sqrt(best);
}
void draw_link(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 col, float thickness) {
    ImVec2 c0, c1;
    link_control_points(p0, p1, c0, c1);
    dl->AddBezierCubic(p0, c0, c1, p1, col, thickness);
}
// Grid-space size of one background grid cell -- also the snap increment applied to node
// positions while dragging, so dragged nodes land exactly on the drawn grid lines instead of some
// finer sub-grid that visually doesn't line up with them.
constexpr float kGridSize = 64.f;
} // namespace

ImVec2 NodeCanvasImgui::grid_to_screen(ImVec2 g) const {
    return ImVec2(canvas_screen_origin_.x + (g.x - pan_.x) * zoom_,
                  canvas_screen_origin_.y + (g.y - pan_.y) * zoom_);
}
ImVec2 NodeCanvasImgui::screen_to_grid(ImVec2 s) const {
    return ImVec2((s.x - canvas_screen_origin_.x) / zoom_ + pan_.x,
                  (s.y - canvas_screen_origin_.y) / zoom_ + pan_.y);
}

bool NodeCanvasImgui::is_node_selected(int node_id) const {
    for (int id : selected_nodes_)
        if (id == node_id)
            return true;
    return false;
}

void NodeCanvasImgui::select_node_on_click(int node_id, bool shift) {
    bool already = is_node_selected(node_id);
    if (shift) {
        if (!already)
            selected_nodes_.push_back(node_id);
        // already selected + shift-click: leave the group as-is so the drag that follows moves it.
    } else if (!already) {
        selected_nodes_.clear();
        selected_nodes_.push_back(node_id);
    }
    // else: no-shift click on a node that's already part of a multi-selection -- keep the whole
    // group selected so this click's drag moves everything together.
    selected_link_ = -1;
}

void NodeCanvasImgui::begin_canvas(const char* str_id, ImVec2 size) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging_node_id_ = -1;
        drag_raw_pos_.clear();
    }
    // box_select_active_ is deliberately NOT reset here: end_canvas() needs to see it still true
    // on the release frame so it can finalize the selection against this frame's node rects before
    // clearing it itself. Resetting it here would race that -- this function runs before
    // end_canvas() within the same frame, so IsMouseDown() is already false on the very frame the
    // selection should resolve.

    ImGui::PushID(str_id);
    ImGui::BeginChild("##node_canvas", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    drawlist_ = ImGui::GetWindowDrawList();
    canvas_screen_origin_ = ImGui::GetCursorScreenPos();
    canvas_size_ = ImGui::GetContentRegionAvail();
    if (canvas_size_.x < 1.f) canvas_size_.x = 1.f;
    if (canvas_size_.y < 1.f) canvas_size_.y = 1.f;

    pins_this_frame_.clear();
    node_rects_this_frame_.clear();
    hovered_node_ = -1;
    link_created_ = false;
    created_start_pin_ = -1;
    created_end_pin_ = -1;
    link_destroyed_ = false;
    destroyed_link_id_ = -1;
    bg_right_clicked_ = false;
    link_dropped_on_empty_ = false;
    frame_hovered_link_ = -1;
    frame_hovered_link_dist_ = 1e9f;

    ImGui::SetCursorScreenPos(canvas_screen_origin_);
    // Needs explicit button flags -- InvisibleButton only tracks the left button by default, so
    // without these ImGui::IsItemActive() below never goes true for a middle-mouse drag and pan
    // silently does nothing (zoom still worked since that's wheel-driven, not button-driven).
    ImGui::InvisibleButton("##bg", canvas_size_,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
    canvas_hovered_ = ImGui::IsItemHovered();
    bool bg_active = ImGui::IsItemActive();

    drawlist_->PushClipRect(canvas_screen_origin_,
                             ImVec2(canvas_screen_origin_.x + canvas_size_.x, canvas_screen_origin_.y + canvas_size_.y),
                             true);

    // background grid, spacing scales with zoom
    {
        float grid_step = kGridSize * zoom_;
        if (grid_step > 8.f) {
            float ox = std::fmod(-pan_.x * zoom_, grid_step);
            float oy = std::fmod(-pan_.y * zoom_, grid_step);
            ImU32 col = IM_COL32(255, 255, 255, 18);
            for (float x = ox; x < canvas_size_.x; x += grid_step)
                drawlist_->AddLine(ImVec2(canvas_screen_origin_.x + x, canvas_screen_origin_.y),
                                    ImVec2(canvas_screen_origin_.x + x, canvas_screen_origin_.y + canvas_size_.y), col);
            for (float y = oy; y < canvas_size_.y; y += grid_step)
                drawlist_->AddLine(ImVec2(canvas_screen_origin_.x, canvas_screen_origin_.y + y),
                                    ImVec2(canvas_screen_origin_.x + canvas_size_.x, canvas_screen_origin_.y + y), col);
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    // zoom on mouse wheel, keeping the point under the cursor fixed
    if (canvas_hovered_ && io.MouseWheel != 0.f) {
        ImVec2 grid_under_mouse = screen_to_grid(io.MousePos);
        zoom_ = std::clamp(zoom_ * (1.f + io.MouseWheel * 0.1f), 0.15f, 3.0f);
        pan_ = ImVec2(grid_under_mouse.x - (io.MousePos.x - canvas_screen_origin_.x) / zoom_,
                      grid_under_mouse.y - (io.MousePos.y - canvas_screen_origin_.y) / zoom_);
    }

    // pan via middle-mouse drag
    if (bg_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        pan_.x -= io.MouseDelta.x / zoom_;
        pan_.y -= io.MouseDelta.y / zoom_;
    }
}

void NodeCanvasImgui::begin_node(int node_id, ImVec2& pos, ImVec2 size, const char* title, ImU32 category_tint) {
    ImGuiIO& io = ImGui::GetIO();
    current_node_id_ = node_id;
    node_input_count_ = 0;
    node_output_count_ = 0;
    body_click_candidate_ = false;

    ImVec2 screen_pos = grid_to_screen(pos);
    ImVec2 screen_size = ImVec2(size.x * zoom_, size.y * zoom_);
    current_node_screen_pos_ = screen_pos;
    current_node_screen_size_ = screen_size;

    float title_h = ImGui::GetFontSize() * zoom_ + 8.f * zoom_;
    ImVec2 p_min = screen_pos;
    ImVec2 p_max = ImVec2(screen_pos.x + screen_size.x, screen_pos.y + screen_size.y);
    ImVec2 title_max = ImVec2(p_max.x, p_min.y + title_h);

    node_rects_this_frame_[node_id] = NodeRect{p_min, p_max};

    bool node_hovered = canvas_hovered_ && ImGui::IsMouseHoveringRect(p_min, p_max, false);
    bool title_hovered = canvas_hovered_ && ImGui::IsMouseHoveringRect(p_min, title_max, false);
    if (node_hovered)
        hovered_node_ = node_id;
    last_begin_node_hovered_ = node_hovered;

    bool selected = is_node_selected(node_id);
    ImU32 body_col = IM_COL32(45, 45, 50, 230);
    ImU32 title_col = category_tint;
    ImU32 border_col = selected ? IM_COL32(255, 200, 80, 255) : IM_COL32(20, 20, 20, 255);

    if (!titlebar_tex_load_attempted_) {
        titlebar_tex_ = g_assets.find<Texture>("eng/editor/node_titlebar2.png").get();
        titlebar_tex_load_attempted_ = true;
    }

    drawlist_->AddRectFilled(p_min, p_max, body_col, 4.f * zoom_);
    if (titlebar_tex_) {
        ImTextureID tex_id = ImTextureID(uint64_t(titlebar_tex_->get_internal_render_handle()));
        drawlist_->AddImageRounded(tex_id, p_min, title_max, ImVec2(0, 0), ImVec2(1, 1), title_col,
                                    4.f * zoom_, ImDrawFlags_RoundCornersTop);
    } else {
        drawlist_->AddRectFilled(p_min, title_max, title_col, 4.f * zoom_, ImDrawFlags_RoundCornersTop);
    }
    drawlist_->AddRect(p_min, p_max, border_col, 4.f * zoom_, 0, selected ? 2.f : 1.f);

    float font_size = ImGui::GetFontSize() * zoom_;
    if (font_size >= 4.f)
        drawlist_->AddText(nullptr, font_size, ImVec2(p_min.x + 6.f * zoom_, p_min.y + 4.f * zoom_),
                            IM_COL32(240, 240, 240, 255), title);

    bool click_free = dragging_node_id_ == -1 && dragging_from_pin_ == -1 && !box_select_active_;
    if (title_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && click_free) {
        select_node_on_click(node_id, io.KeyShift);
        dragging_node_id_ = node_id;
    } else if (node_hovered && !title_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && click_free) {
        // Deferred: pins are drawn after begin_node, so we don't yet know whether this click
        // actually landed on a pin (which should start a link-drag, not a node-drag). Resolved in
        // end_node() once pin() calls for this node have had a chance to claim it first.
        body_click_candidate_ = true;
    }
    if (dragging_node_id_ != -1 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && is_node_selected(node_id)) {
        // Snap is derived from an unsnapped accumulator (drag_raw_pos_), not from repeatedly
        // re-rounding `pos` itself -- rounding the live position in place compounds error frame to
        // frame and lets the node drift away from the cursor as the drag goes on.
        auto it = drag_raw_pos_.find(node_id);
        ImVec2 raw = (it != drag_raw_pos_.end()) ? it->second : pos;
        if (it != drag_raw_pos_.end()) {
            raw.x += io.MouseDelta.x / zoom_;
            raw.y += io.MouseDelta.y / zoom_;
        }
        drag_raw_pos_[node_id] = raw;
        pos.x = std::round(raw.x / kGridSize) * kGridSize;
        pos.y = std::round(raw.y / kGridSize) * kGridSize;
    }
}

void NodeCanvasImgui::end_node() {
    if (body_click_candidate_ && dragging_node_id_ == -1 && dragging_from_pin_ == -1) {
        select_node_on_click(current_node_id_, ImGui::GetIO().KeyShift);
        dragging_node_id_ = current_node_id_;
    }
    body_click_candidate_ = false;
    current_node_id_ = -1;
}

void NodeCanvasImgui::pin(int pin_id, bool is_output, const char* label, ImU32 color) {
    int& count = is_output ? node_output_count_ : node_input_count_;
    float title_h = ImGui::GetFontSize() * zoom_ + 8.f * zoom_;
    float spacing = 20.f * zoom_;
    float y = current_node_screen_pos_.y + title_h + spacing * count + spacing * 0.5f;
    float x = is_output ? (current_node_screen_pos_.x + current_node_screen_size_.x) : current_node_screen_pos_.x;
    count++;

    ImVec2 center(x, y);
    float radius = std::max(5.f * zoom_, 2.f);

    ImGuiIO& io = ImGui::GetIO();
    float dx = io.MousePos.x - center.x, dy = io.MousePos.y - center.y;
    float hit_radius = radius * 3.f;
    bool pin_hovered = canvas_hovered_ && (dx * dx + dy * dy) <= hit_radius * hit_radius;

    drawlist_->AddCircleFilled(center, radius, pin_hovered ? IM_COL32(255, 255, 255, 255) : color);
    drawlist_->AddCircle(center, radius, IM_COL32(0, 0, 0, 180));

    float font_size = ImGui::GetFontSize() * zoom_;
    if (font_size >= 4.f && label && label[0]) {
        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos = is_output
            ? ImVec2(center.x - radius - 4.f * zoom_ - text_size.x * zoom_, center.y - font_size * 0.5f)
            : ImVec2(center.x + radius + 4.f * zoom_, center.y - font_size * 0.5f);
        drawlist_->AddText(nullptr, font_size, text_pos, IM_COL32(200, 200, 200, 255), label);
    }

    pins_this_frame_[pin_id] = PinInfo{center, is_output, current_node_id_};

    if (pin_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragging_node_id_ == -1 && dragging_from_pin_ == -1) {
        dragging_from_pin_ = pin_id;
        dragging_from_is_output_ = is_output;
        body_click_candidate_ = false; // this node's body click was actually a pin click -- don't also start a node-drag in end_node()
    }
}

void NodeCanvasImgui::link(int link_id, int start_pin_id, int end_pin_id, ImU32 color) {
    auto a = pins_this_frame_.find(start_pin_id);
    auto b = pins_this_frame_.find(end_pin_id);
    if (a == pins_this_frame_.end() || b == pins_this_frame_.end())
        return;

    bool is_selected = (selected_link_ == link_id);

    float dist = distance_to_link(a->second.screen_pos, b->second.screen_pos, ImGui::GetIO().MousePos);
    if (canvas_hovered_ && dist < 6.f && dist < frame_hovered_link_dist_) {
        frame_hovered_link_dist_ = dist;
        frame_hovered_link_ = link_id;
    }

    ImU32 draw_col = is_selected ? IM_COL32(255, 200, 80, 255) : color;
    draw_link(drawlist_, a->second.screen_pos, b->second.screen_pos, draw_col, is_selected ? 3.f : 2.f);
}

void NodeCanvasImgui::end_canvas() {
    ImGuiIO& io = ImGui::GetIO();

    if (dragging_from_pin_ != -1) {
        auto src = pins_this_frame_.find(dragging_from_pin_);
        if (src != pins_this_frame_.end())
            draw_link(drawlist_, src->second.screen_pos, io.MousePos, IM_COL32(255, 255, 255, 180), 2.f);

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            int best = -1;
            float best_d2 = 15.f * 15.f;
            int src_node = src != pins_this_frame_.end() ? src->second.node_id : -1;
            for (auto& kv : pins_this_frame_) {
                if (kv.first == dragging_from_pin_) continue;
                if (kv.second.is_output == dragging_from_is_output_) continue; // output<->input only
                if (kv.second.node_id == src_node) continue;                  // no self-loops
                float dx = kv.second.screen_pos.x - io.MousePos.x, dy = kv.second.screen_pos.y - io.MousePos.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < best_d2) { best_d2 = d2; best = kv.first; }
            }
            if (best != -1) {
                link_created_ = true;
                if (dragging_from_is_output_) { created_start_pin_ = dragging_from_pin_; created_end_pin_ = best; }
                else { created_start_pin_ = best; created_end_pin_ = dragging_from_pin_; }
            } else if (canvas_hovered_) {
                link_dropped_on_empty_ = true;
                dropped_from_pin_ = dragging_from_pin_;
                dropped_grid_pos_ = screen_to_grid(io.MousePos);
            }
            dragging_from_pin_ = -1;
        }
    }

    if (frame_hovered_link_ != -1 && canvas_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        dragging_node_id_ == -1 && dragging_from_pin_ == -1) {
        selected_link_ = frame_hovered_link_;
        selected_nodes_.clear();
    }
    if (selected_link_ != -1 && !io.WantTextInput &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        link_destroyed_ = true;
        destroyed_link_id_ = selected_link_;
        selected_link_ = -1;
    }

    // Background right-click: only fires when the click didn't land on any node (hovered_node_ is
    // only known for certain now, after every begin_node() this frame has run).
    if (canvas_hovered_ && hovered_node_ == -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
        dragging_node_id_ == -1 && dragging_from_pin_ == -1) {
        bg_right_clicked_ = true;
        bg_right_click_grid_pos_ = screen_to_grid(io.MousePos);
    }

    // Box select: starts on an empty-background left click (same "wait until nodes are known"
    // reasoning as above), grows while held, resolves against every node's screen rect on release.
    if (!box_select_active_ && canvas_hovered_ && hovered_node_ == -1 && frame_hovered_link_ == -1 &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragging_node_id_ == -1 && dragging_from_pin_ == -1) {
        box_select_active_ = true;
        box_select_start_screen_ = io.MousePos;
        box_select_additive_ = io.KeyShift;
    }
    if (box_select_active_) {
        ImVec2 a = box_select_start_screen_, b = io.MousePos;
        ImVec2 lo(std::min(a.x, b.x), std::min(a.y, b.y));
        ImVec2 hi(std::max(a.x, b.x), std::max(a.y, b.y));
        drawlist_->AddRectFilled(lo, hi, IM_COL32(100, 150, 255, 40));
        drawlist_->AddRect(lo, hi, IM_COL32(100, 150, 255, 200));

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (!box_select_additive_)
                selected_nodes_.clear();
            for (auto& kv : node_rects_this_frame_) {
                bool intersects = kv.second.min.x <= hi.x && kv.second.max.x >= lo.x &&
                                   kv.second.min.y <= hi.y && kv.second.max.y >= lo.y;
                if (intersects && !is_node_selected(kv.first))
                    selected_nodes_.push_back(kv.first);
            }
            box_select_active_ = false;
        }
    }

    // F: fit pan/zoom to show every node laid out this frame. node_rects_this_frame_ is in screen
    // space (this frame's pan/zoom already baked in via grid_to_screen()), so it's converted back
    // to grid space before computing the new view -- that's what makes the fit independent of
    // whatever pan/zoom happened to be in effect when F was pressed.
    if (canvas_hovered_ && !io.WantTextInput && !node_rects_this_frame_.empty() &&
        ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        auto it = node_rects_this_frame_.begin();
        ImVec2 lo = screen_to_grid(it->second.min);
        ImVec2 hi = screen_to_grid(it->second.max);
        for (++it; it != node_rects_this_frame_.end(); ++it) {
            ImVec2 gmin = screen_to_grid(it->second.min);
            ImVec2 gmax = screen_to_grid(it->second.max);
            lo.x = std::min(lo.x, gmin.x); lo.y = std::min(lo.y, gmin.y);
            hi.x = std::max(hi.x, gmax.x); hi.y = std::max(hi.y, gmax.y);
        }
        ImVec2 center((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
        const float margin = 1.15f; // leaves a little breathing room around the outermost nodes
        float extent_x = std::max(hi.x - lo.x, 1.f) * margin;
        float extent_y = std::max(hi.y - lo.y, 1.f) * margin;
        zoom_ = std::clamp(std::min(canvas_size_.x / extent_x, canvas_size_.y / extent_y), 0.15f, 3.0f);
        pan_ = ImVec2(center.x - (canvas_size_.x * 0.5f) / zoom_, center.y - (canvas_size_.y * 0.5f) / zoom_);
    }

    drawlist_->PopClipRect();
    ImGui::EndChild();
    ImGui::PopID();
}

bool NodeCanvasImgui::is_link_created(int* start_pin_id, int* end_pin_id) const {
    if (!link_created_) return false;
    if (start_pin_id) *start_pin_id = created_start_pin_;
    if (end_pin_id) *end_pin_id = created_end_pin_;
    return true;
}
bool NodeCanvasImgui::is_link_destroyed(int* link_id) const {
    if (!link_destroyed_) return false;
    if (link_id) *link_id = destroyed_link_id_;
    return true;
}
bool NodeCanvasImgui::is_background_right_clicked(ImVec2* out_grid_pos) const {
    if (!bg_right_clicked_) return false;
    if (out_grid_pos) *out_grid_pos = bg_right_click_grid_pos_;
    return true;
}
bool NodeCanvasImgui::is_link_dropped_on_empty(int* out_pin_id, ImVec2* out_grid_pos) const {
    if (!link_dropped_on_empty_) return false;
    if (out_pin_id) *out_pin_id = dropped_from_pin_;
    if (out_grid_pos) *out_grid_pos = dropped_grid_pos_;
    return true;
}
