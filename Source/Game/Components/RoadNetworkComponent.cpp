#include "RoadNetworkComponent.h"
#include "Render/DrawPublic.h"
#include "Render/ModelManager.h"
#include "Game/Entity.h"
#include "Framework/Serializer.h"
#include "Physics/Physics2.h"

#include <glm/gtx/norm.hpp>
#include <cmath>
#include <algorithm>

static constexpr int   ROAD_SEGMENTS      = 20;    // subdivisions per edge for terrain following
static constexpr float ROAD_GROUND_OFFSET = 0.08f; // lift above terrain surface

// ---- helpers ----------------------------------------------------------------

static glm::vec3 bezier_cubic(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t)
{
    float u = 1.f - t;
    return u*u*u*p0 + 3.f*u*u*t*p1 + 3.f*u*t*t*p2 + t*t*t*p3;
}

// Downward raycast to find terrain height at x/z; returns pt with y snapped + offset.
// Falls back to the original y + offset if terrain is not hit.
static glm::vec3 snap_to_terrain(glm::vec3 pt)
{
    world_query_result res;
    glm::vec3 from = glm::vec3(pt.x, pt.y + 200.f, pt.z);
    glm::vec3 to   = glm::vec3(pt.x, pt.y - 200.f, pt.z);
    if (g_physics.trace_ray(res, from, to, nullptr, UINT32_MAX))
        return glm::vec3(pt.x, res.hit_pos.y + ROAD_GROUND_OFFSET, pt.z);
    return glm::vec3(pt.x, pt.y + ROAD_GROUND_OFFSET, pt.z);
}

// Build a quad strip between a sequence of center-line points
static void push_road_strip(ModelBuilder& builder,
                             const std::vector<glm::vec3>& pts,
                             float half_w)
{
    if (pts.size() < 2) return;

    const glm::vec3 up(0.f, 1.f, 0.f);

    // Cumulative arc-length for UV v-coord (tiles once per road-width so no stretching)
    std::vector<float> cum(pts.size(), 0.f);
    for (int i = 1; i < (int)pts.size(); i++)
        cum[i] = cum[i-1] + glm::distance(pts[i], pts[i-1]);
    const float tile_len = half_w * 2.f > 0.001f ? half_w * 2.f : 1.f;

    uint16_t prev_l = 0, prev_r = 0;
    for (int i = 0; i < (int)pts.size(); i++) {
        glm::vec3 dir;
        if (i == 0)
            dir = glm::normalize(pts[1] - pts[0]);
        else if (i == (int)pts.size()-1)
            dir = glm::normalize(pts[i] - pts[i-1]);
        else
            dir = glm::normalize(pts[i+1] - pts[i-1]);

        glm::vec3 right = glm::normalize(glm::cross(dir, up)) * half_w;
        float v = cum[i] / tile_len;

        uint16_t vl = builder.add_vertex(pts[i] - right, {0.f, v}, up);
        uint16_t vr = builder.add_vertex(pts[i] + right, {1.f, v}, up);

        if (i > 0) {
            // quad: prev_l, prev_r, vr, vl  (CCW from above)
            builder.add_quad(prev_l, prev_r, vr, vl);
        }
        prev_l = vl;
        prev_r = vr;
    }
}

// ---- RoadNetworkComponent ---------------------------------------------------

RoadNetworkComponent::RoadNetworkComponent()
{
    set_call_init_in_editor(true);
}

RoadNetworkComponent::~RoadNetworkComponent() = default;

void RoadNetworkComponent::start()
{
    rebuild_mesh();
}

void RoadNetworkComponent::stop()
{
    dynamic_model.reset();
    idraw->get_scene()->remove_obj(render_handle);
}

void RoadNetworkComponent::on_changed_transform()
{
    if (render_handle.is_valid()) {
        Render_Object ro;
        ro.model    = dynamic_model.get();
        ro.visible  = dynamic_model != nullptr;
        ro.shadow_caster = false;
        ro.transform = glm::mat4(1.f);
        idraw->get_scene()->update_obj(render_handle, ro);
    }
}

void RoadNetworkComponent::serialize(Serializer& s)
{
    Component::serialize(s);

    int32_t n_count = (int32_t)nodes.size();
    if (s.serialize_array("nodes", n_count)) {
        if (s.is_loading()) {
            nodes.resize(n_count);
            for (auto& n : nodes) n = RoadNode{};
        }
        for (int i = 0; i < n_count; i++) {
            if (s.serialize_dict_ar()) {
                s.serialize("id",  nodes[i].id);
                s.serialize("pos", nodes[i].position);
                s.end_obj();
            }
        }
        s.end_obj();
    }

    int32_t e_count = (int32_t)edges.size();
    if (s.serialize_array("edges", e_count)) {
        if (s.is_loading()) {
            edges.resize(e_count);
            for (auto& e : edges) e = RoadEdge{};
        }
        for (int i = 0; i < e_count; i++) {
            if (s.serialize_dict_ar()) {
                s.serialize("id",      edges[i].id);
                s.serialize("na",      edges[i].node_a_id);
                s.serialize("nb",      edges[i].node_b_id);
                s.serialize("width",   edges[i].width);
                s.serialize("curved",  edges[i].curved);
                s.serialize("ctrl_a",  edges[i].ctrl_a);
                s.serialize("ctrl_b",  edges[i].ctrl_b);
                s.end_obj();
            }
        }
        s.end_obj();
    }

    s.serialize("nxt_nid", next_node_id);
    s.serialize("nxt_eid", next_edge_id);

    if (s.is_loading())
        rebuild_mesh();
}

// ---- Graph mutation ---------------------------------------------------------

int RoadNetworkComponent::add_node(glm::vec3 position)
{
    RoadNode n;
    n.id       = next_node_id++;
    n.position = position;
    nodes.push_back(n);
    return n.id;
}

void RoadNetworkComponent::remove_node(int node_id)
{
    // Remove all edges referencing this node
    edges.erase(std::remove_if(edges.begin(), edges.end(),
        [node_id](const RoadEdge& e){
            return e.node_a_id == node_id || e.node_b_id == node_id;
        }), edges.end());

    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
        [node_id](const RoadNode& n){ return n.id == node_id; }), nodes.end());
}

int RoadNetworkComponent::add_edge(int node_a_id, int node_b_id, float width, bool curved)
{
    if (edge_exists(node_a_id, node_b_id)) return -1;

    auto* na = find_node(node_a_id);
    auto* nb = find_node(node_b_id);
    if (!na || !nb) return -1;

    RoadEdge e;
    e.id         = next_edge_id++;
    e.node_a_id  = node_a_id;
    e.node_b_id  = node_b_id;
    e.width      = width;
    e.curved     = curved;

    // Default control points: 1/3 along the straight line from each end
    glm::vec3 delta = nb->position - na->position;
    e.ctrl_a = na->position + delta * (1.f/3.f);
    e.ctrl_b = na->position + delta * (2.f/3.f);

    edges.push_back(e);
    return e.id;
}

void RoadNetworkComponent::remove_edge(int edge_id)
{
    edges.erase(std::remove_if(edges.begin(), edges.end(),
        [edge_id](const RoadEdge& e){ return e.id == edge_id; }), edges.end());
}

void RoadNetworkComponent::move_node(int node_id, glm::vec3 new_pos)
{
    if (auto* n = find_node(node_id))
        n->position = new_pos;
}

void RoadNetworkComponent::set_edge_control_points(int edge_id, glm::vec3 ctrl_a, glm::vec3 ctrl_b)
{
    if (auto* e = find_edge(edge_id)) {
        e->ctrl_a = ctrl_a;
        e->ctrl_b = ctrl_b;
    }
}

// ---- Graph queries ----------------------------------------------------------

RoadNode* RoadNetworkComponent::find_node(int id)
{
    for (auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
const RoadNode* RoadNetworkComponent::find_node(int id) const
{
    for (const auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
RoadEdge* RoadNetworkComponent::find_edge(int id)
{
    for (auto& e : edges)
        if (e.id == id) return &e;
    return nullptr;
}
const RoadEdge* RoadNetworkComponent::find_edge(int id) const
{
    for (const auto& e : edges)
        if (e.id == id) return &e;
    return nullptr;
}

bool RoadNetworkComponent::edge_exists(int a, int b) const
{
    for (const auto& e : edges)
        if ((e.node_a_id == a && e.node_b_id == b) ||
            (e.node_a_id == b && e.node_b_id == a))
            return true;
    return false;
}

// ---- Mesh generation --------------------------------------------------------

void RoadNetworkComponent::rebuild_mesh()
{
    ModelBuilder builder;
    const glm::vec3 up(0.f, 1.f, 0.f);

    // --- Road segments ---
    for (const auto& edge : edges) {
        const RoadNode* na = find_node(edge.node_a_id);
        const RoadNode* nb = find_node(edge.node_b_id);
        if (!na || !nb) continue;

        float half_w = edge.width * 0.5f;

        // Sample centerline (both straight and curved use ROAD_SEGMENTS subdivisions
        // so downward raycasts can conform each point to terrain)
        std::vector<glm::vec3> pts;
        pts.reserve(ROAD_SEGMENTS + 1);
        for (int i = 0; i <= ROAD_SEGMENTS; i++) {
            float t = float(i) / float(ROAD_SEGMENTS);
            glm::vec3 p = edge.curved
                ? bezier_cubic(na->position, edge.ctrl_a, edge.ctrl_b, nb->position, t)
                : glm::mix(na->position, nb->position, t);
            pts.push_back(snap_to_terrain(p));
        }

        push_road_strip(builder, pts, half_w);
    }

    // --- Intersection caps ---
    // For each node with 2+ connections generate a circle to fill the junction.
    for (const auto& node : nodes) {
        std::vector<const RoadEdge*> conn;
        for (const auto& e : edges)
            if (e.node_a_id == node.id || e.node_b_id == node.id)
                conn.push_back(&e);

        if (conn.size() < 2) continue; // dead-end, road strip already covers it

        float max_hw = 0.f;
        for (auto* e : conn)
            max_hw = std::max(max_hw, e->width * 0.5f);

        // Snap center and rim verts to terrain; add a small extra lift so the cap
        // sits just above the road strips that meet here.
        static constexpr float CAP_EXTRA = 0.02f;
        glm::vec3 cap_origin = snap_to_terrain(node.position);
        cap_origin.y += CAP_EXTRA;

        const int N = 16;
        uint16_t center = builder.add_vertex(cap_origin, {0.5f, 0.5f}, up);
        uint16_t first = 0, prev = 0;
        for (int i = 0; i < N; i++) {
            float angle = float(i) / float(N) * 2.f * PI;
            glm::vec3 rim_xz = node.position + glm::vec3(std::cos(angle) * max_hw, 0.f, std::sin(angle) * max_hw);
            glm::vec3 rim    = snap_to_terrain(rim_xz);
            rim.y += CAP_EXTRA;
            glm::vec2 uv = { std::cos(angle) * 0.5f + 0.5f, std::sin(angle) * 0.5f + 0.5f };
            uint16_t v = builder.add_vertex(rim, uv, up);
            if (i == 0) first = v;
            // Winding: (center, v, prev) so normal faces up (+Y)
            if (i > 0) builder.add_triangle(center, v, prev);
            prev = v;
        }
        builder.add_triangle(center, first, prev);
    }

    // Upload / refresh GPU model
    if (builder.get_vertex_count() == 0) {
        // No geometry – release model and hide render object
        dynamic_model.reset();
        if (render_handle.is_valid()) {
            idraw->get_scene()->remove_obj(render_handle);
        }
        return;
    }

    if (!dynamic_model) {
        dynamic_model.reset(g_modelMgr.create_dynamic_model(builder, "road_network"));
    } else {
        g_modelMgr.refresh_dynamic_model(dynamic_model.get(), builder);
    }

    if (!render_handle.is_valid())
        render_handle = idraw->get_scene()->register_obj();

    Render_Object ro;
    ro.model        = dynamic_model.get();
    ro.visible      = true;
    ro.shadow_caster = false;
    ro.transform    = glm::mat4(1.f);
    ro.owner        = this;
    idraw->get_scene()->update_obj(render_handle, ro);
}
