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

// Screen-space radius for node hit-testing (pixels)
static constexpr float NODE_HIT_RADIUS = 12.f;
// Pixel threshold for edge hit-testing
static constexpr float EDGE_HIT_DIST   = 10.f;

// ---- construction -----------------------------------------------------------

RoadBuilderTool::RoadBuilderTool(EditorDoc& doc) : doc(doc)
{
    ASSERT(idraw && idraw->get_scene());
    mb_handle = idraw->get_scene()->register_meshbuilder();
}

RoadBuilderTool::~RoadBuilderTool()
{
    ASSERT(idraw && idraw->get_scene());
    idraw->get_scene()->remove_meshbuilder(mb_handle);
}

// ---- helpers ----------------------------------------------------------------

RoadNetworkComponent* RoadBuilderTool::get_or_create_component()
{
    ASSERT(eng);
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
    ASSERT(UiSystem::inst);
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
    ASSERT(eng);
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
    ASSERT(eng);
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
    ASSERT(eng);
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
    ASSERT(doc.get_vs());
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
    ASSERT(true); // snap is always safe to apply; no precondition to assert
    if (!ed_has_snap.get_bool()) return pos;
    float snap = ed_translation_snap.get_float();
    if (snap < 0.001f) return pos;
    pos.x = std::round(pos.x / snap) * snap;
    pos.z = std::round(pos.z / snap) * snap;
    return pos;
}

#endif // EDITOR_BUILD
