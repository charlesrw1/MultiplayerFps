#pragma once
#include "imgui.h"
#include <unordered_map>
#include <vector>

class Texture; // Render/Texture.h -- lazily loaded for the node titlebar background, see begin_node()

// Minimal immediate-mode node-graph canvas with pan + zoom built in from the start.
// Not tied to any graph data model -- caller owns node/pin/link storage and integer ids;
// this widget only draws them and reports interaction events for the current frame.
//
// Usage per frame:
//   canvas.begin_canvas("MyGraph");
//   for (auto& n : my_nodes) {
//       canvas.begin_node(n.id, n.pos, n.size, n.title.c_str());
//       for (auto& p : n.inputs)  canvas.pin(p.id, false, p.label.c_str());
//       for (auto& p : n.outputs) canvas.pin(p.id, true,  p.label.c_str());
//       canvas.end_node();
//   }
//   for (auto& l : my_links) canvas.link(l.id, l.start_pin, l.end_pin);
//   canvas.end_canvas();
//   int a, b; if (canvas.is_link_created(&a, &b)) my_links.push_back({next_id++, a, b});
//   int dead; if (canvas.is_link_destroyed(&dead)) erase_link(dead);
//   ImVec2 p; if (canvas.is_background_right_clicked(&p)) open_add_node_popup_at(p);
class NodeCanvasImgui
{
public:
    void begin_canvas(const char* str_id, ImVec2 size = ImVec2(0, 0));
    void end_canvas();

    // pos/size are in grid space; pos is mutated in place while the node (or, if node_id is part
    // of a multi-node selection, the whole selection) is dragged. category_tint colors the
    // titlebar (tinting the titlebar image if one loaded, or used directly as the fill color
    // otherwise) -- callers can use it to color-code nodes by kind; selection state is shown via
    // the border instead, so it stays visible regardless of category color.
    void begin_node(int node_id, ImVec2& pos, ImVec2 size, const char* title, ImU32 category_tint = IM_COL32(60, 60, 66, 255));
    void end_node();

    // call between begin_node/end_node. is_output pins render on the right edge, inputs on the left.
    void pin(int pin_id, bool is_output, const char* label, ImU32 color = IM_COL32(210, 210, 210, 255));

    // call after all begin_node/end_node calls for the frame.
    void link(int link_id, int start_pin_id, int end_pin_id, ImU32 color = IM_COL32(200, 200, 100, 255));

    // valid after end_canvas(); true (once) the frame a new link is dragged out between two pins.
    bool is_link_created(int* start_pin_id, int* end_pin_id) const;
    // valid after end_canvas(); true (once) the frame the user deletes the selected link.
    bool is_link_destroyed(int* link_id) const;
    // valid after end_canvas(); true (once) the frame a link dragged out of a pin is released over
    // empty canvas space (not on any other pin). *out_pin_id is the pin the drag started from,
    // *out_grid_pos is where it was released -- typically used to open a "create + auto-connect"
    // node picker at that position.
    bool is_link_dropped_on_empty(int* out_pin_id, ImVec2* out_grid_pos) const;
    // valid after end_canvas(); true (once) the frame the user right-clicks empty canvas space
    // (i.e. not on top of any node). *out_grid_pos is where the click landed, in grid space --
    // pass straight through as the new node's initial editor_pos.
    bool is_background_right_clicked(ImVec2* out_grid_pos) const;

    int get_selected_node() const { return selected_nodes_.empty() ? -1 : selected_nodes_.back(); }
    const std::vector<int>& get_selected_nodes() const { return selected_nodes_; }
    bool is_node_selected(int node_id) const;
    int get_selected_link() const { return selected_link_; }
    int get_hovered_node() const { return hovered_node_; }
    // valid immediately after the begin_node() call it corresponds to (unlike get_hovered_node(),
    // which reflects whichever node was hovered last across the whole frame) -- use this right
    // after begin_node() to react to a click/right-click on that specific node.
    bool is_last_node_hovered() const { return last_begin_node_hovered_; }

    void reset_view() { zoom_ = 1.f; pan_ = ImVec2(0, 0); }
    float get_zoom() const { return zoom_; }

private:
    ImVec2 grid_to_screen(ImVec2 g) const;
    ImVec2 screen_to_grid(ImVec2 s) const;
    // Click on node_id (title or body) resolved to a selection change: replaces the selection
    // with just this node unless shift is held (adds) or this node is already part of a
    // multi-selection (keeps the whole group selected, so the drag that follows moves all of it).
    void select_node_on_click(int node_id, bool shift);

    struct PinInfo {
        ImVec2 screen_pos;
        bool is_output;
        int node_id;
    };
    struct NodeRect {
        ImVec2 min, max;
    };

    // canvas-wide state (persists across frames)
    float zoom_ = 1.f;
    ImVec2 pan_ = ImVec2(0, 0); // grid-space point that sits at the canvas origin

    int dragging_node_id_ = -1; // drag anchor; while set, every node in selected_nodes_ is moved
    int dragging_from_pin_ = -1;
    bool dragging_from_is_output_ = false;
    // Per-node unsnapped drag position, accumulated fresh each drag session (cleared when the drag
    // ends). Grid-snapping is derived from this every frame rather than repeatedly re-snapping the
    // already-snapped `pos` -- snapping the live position in place compounds rounding error frame
    // to frame, which made the snap feel jumpy/inconsistent and let the node drift away from the
    // cursor over the course of a drag.
    std::unordered_map<int, ImVec2> drag_raw_pos_;

    std::vector<int> selected_nodes_;
    int selected_link_ = -1;

    bool box_select_active_ = false;
    ImVec2 box_select_start_screen_{};
    bool box_select_additive_ = false; // shift was held when the drag started

    // per-frame scratch (cleared in begin_canvas)
    ImDrawList* drawlist_ = nullptr;
    ImVec2 canvas_screen_origin_{};
    ImVec2 canvas_size_{};
    std::unordered_map<int, PinInfo> pins_this_frame_;
    std::unordered_map<int, NodeRect> node_rects_this_frame_;
    bool canvas_hovered_ = false;
    int hovered_node_ = -1;
    bool last_begin_node_hovered_ = false;
    int current_node_id_ = -1;
    ImVec2 current_node_screen_pos_{};
    ImVec2 current_node_screen_size_{};
    int node_input_count_ = 0;
    int node_output_count_ = 0;
    bool body_click_candidate_ = false; // this node's body (not title) was clicked; resolved in end_node once pins have had a chance to claim the click

    bool link_created_ = false;
    int created_start_pin_ = -1;
    int created_end_pin_ = -1;
    bool link_destroyed_ = false;
    int destroyed_link_id_ = -1;

    bool bg_right_clicked_ = false;
    ImVec2 bg_right_click_grid_pos_{};

    bool link_dropped_on_empty_ = false;
    int dropped_from_pin_ = -1;
    ImVec2 dropped_grid_pos_{};

    // hit-testing links this frame: nearest link under the mouse + its distance
    int frame_hovered_link_ = -1;
    float frame_hovered_link_dist_ = 0.f;

    // Non-owning -- the asset registry owns it. Lazily resolved on first begin_node() call rather
    // than in a constructor since this class has none; g_assets.find() is registry-cached so the
    // lazy lookup only actually costs anything once. Left null (falls back to a flat fill) if the
    // asset is missing rather than asserting -- it's cosmetic, not load-bearing.
    Texture* titlebar_tex_ = nullptr;
    bool titlebar_tex_load_attempted_ = false;
};
