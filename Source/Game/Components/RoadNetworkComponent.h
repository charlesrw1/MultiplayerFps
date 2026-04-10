#pragma once

#include "Game/EntityComponent.h"
#include "Render/DynamicModelPtr.h"
#include "Render/RenderObj.h"
#include "Framework/Handle.h"
#include <vector>

// A single junction/endpoint in the road graph
struct RoadNode {
    int32_t id = 0;
    glm::vec3 position = glm::vec3(0.f);
};

// A road segment connecting two RoadNodes
struct RoadEdge {
    int32_t id = 0;
    int32_t node_a_id = -1;
    int32_t node_b_id = -1;
    float width = 6.0f;
    bool curved = false;
    // Cubic Bezier control points (only used when curved == true)
    glm::vec3 ctrl_a = glm::vec3(0.f); // near node_a
    glm::vec3 ctrl_b = glm::vec3(0.f); // near node_b
};

/// Component that owns a road network graph and generates/renders a procedural
/// road mesh. Save/load is handled via the standard Component::serialize() override.
/// Use RoadBuilderTool (IEditorMode) to edit roads interactively.
class RoadNetworkComponent : public Component
{
public:
    CLASS_BODY(RoadNetworkComponent);

    RoadNetworkComponent();
    ~RoadNetworkComponent() override;

    // Component lifecycle
    void start() override;
    void stop()  override;
    void on_changed_transform() override;
    void serialize(Serializer& s) override;

    // ---- Graph mutation API (called by editor tool) ----
    int  add_node(glm::vec3 position);
    void remove_node(int node_id);
    int  add_edge(int node_a_id, int node_b_id, float width = 6.f, bool curved = false);
    void remove_edge(int edge_id);
    void move_node(int node_id, glm::vec3 new_position);
    void set_edge_control_points(int edge_id, glm::vec3 ctrl_a, glm::vec3 ctrl_b);

    // ---- Graph queries ----
    const std::vector<RoadNode>& get_nodes() const { return nodes; }
    const std::vector<RoadEdge>& get_edges() const { return edges; }
    RoadNode* find_node(int id);
    const RoadNode* find_node(int id) const;
    RoadEdge* find_edge(int id);
    const RoadEdge* find_edge(int id) const;

    /// Returns true if an edge already exists between the two nodes (in either direction)
    bool edge_exists(int node_a_id, int node_b_id) const;

    /// Rebuild road mesh from current graph and re-upload to GPU.
    /// Call after any graph mutation.
    void rebuild_mesh();

#ifdef EDITOR_BUILD
    const char* get_editor_outliner_icon() const override { return "eng/editor/mesh_icon.png"; }
#endif

private:
    std::vector<RoadNode> nodes;
    std::vector<RoadEdge> edges;
    int32_t next_node_id = 1;
    int32_t next_edge_id = 1;

    DynamicModelUniquePtr dynamic_model;
    handle<Render_Object>  render_handle;
};
