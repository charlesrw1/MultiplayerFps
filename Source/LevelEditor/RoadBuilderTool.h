#pragma once
#ifdef EDITOR_BUILD
#include "EditorModes.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Framework/Handle.h"

class RoadNetworkComponent;

/// Cities-Skylines-style road network editor.
/// Activated via Mode > Road Builder in the menu bar.
/// Provides: place/drag nodes, connect with straight or curved roads,
/// delete nodes/edges, and snapping.
class RoadBuilderTool : public IEditorMode
{
public:
    enum class SubMode { PlaceNode, Connect, Delete };

    explicit RoadBuilderTool(EditorDoc& doc);
    ~RoadBuilderTool();

    void tick(EditorInputs& inputs) override;
    void draw_ui() override;

private:
    // ---- helpers ----
    RoadNetworkComponent* get_or_create_component();

    // Returns screen-space position of a world point (or off-screen sentinel)
    ImVec2 world_to_screen(glm::vec3 world) const;

    // Hit-test: returns node id under mouse or -1
    int  pick_node(glm::ivec2 mouse_screen) const;
    // Hit-test: returns edge id whose midpoint is near mouse or -1
    int  pick_edge(glm::ivec2 mouse_screen) const;
    // Hit-test: returns true if a curved edge control point is under mouse
    bool pick_ctrl_pt(glm::ivec2 mouse_screen, int& out_edge_id, bool& out_is_a) const;

    // Ground-plane raycast (y = component_y). Returns true on hit.
    bool ray_to_ground(int mx, int my, glm::vec3& out) const;

    // Snap a world position to the editor grid (if snap enabled)
    glm::vec3 apply_snap(glm::vec3 pos) const;

    // Draw the overlay (nodes, edges, preview) using ImGui foreground draw list
    void draw_overlay(const RoadNetworkComponent* net) const;

    // ---- state ----
    EditorDoc& doc;
    SubMode    sub_mode = SubMode::PlaceNode;
    bool       curved   = false;
    float      road_width = 6.f;

    // Connect mode: id of the first selected node (-1 = none)
    int connect_src_id = -1;

    // Node drag state
    bool     dragging     = false;
    int      drag_node_id = -1;

    // Control-point drag state
    int  drag_ctrl_edge_id = -1;
    bool drag_ctrl_is_a    = false;

    // Hovered / selected for visual feedback
    mutable int  hovered_node      = -1;
    mutable int  hovered_edge      = -1;
    mutable int  hovered_ctrl_edge = -1;
    mutable bool hovered_ctrl_is_a = false;

    // MeshBuilder overlay registered in scene (drawn in 3D)
    MeshBuilder              mb;
    handle<MeshBuilder_Object> mb_handle;
};

#endif // EDITOR_BUILD
