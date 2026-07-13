// ModelDynamic.cpp — ModelBuilder helpers and ModelMan dynamic-model API
// Split from Model.cpp to keep file sizes under 1000 LOC.

#include "Model.h"
#include "ModelManager.h"

#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"

#include "Texture.h"

#include "Framework/Files.h"

#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include <algorithm>

#include "Render/MaterialPublic.h"

#include "Render/MaterialLocal.h"
#include "DrawLocal.h"

// ---------------------------------------------------------------------------
// ModelBuilder implementation
// ---------------------------------------------------------------------------

/// Packs a normalized float in [-1, 1] into an int16_t.
static inline int16_t pack_norm(float f) {
    return (int16_t)(f * (float)INT16_MAX);
}

uint16_t ModelBuilder::add_vertex(glm::vec3 pos, glm::vec2 uv, glm::vec3 normal) {
    ASSERT(vertices.size() < (size_t)UINT16_MAX
           && "ModelBuilder vertex limit exceeded (max 65535 per model)");

    ModelVertex v{};
    v.pos = pos;
    v.uv  = uv;

    // Encode surface normal as packed int16.  Zero-length normals fall back
    // to +Y so the vertex is never left uninitialized.
    glm::vec3 n = (glm::length(normal) > 0.f)
                      ? glm::normalize(normal)
                      : glm::vec3(0.f, 1.f, 0.f);
    v.normal[0] = pack_norm(n.x);
    v.normal[1] = pack_norm(n.y);
    v.normal[2] = pack_norm(n.z);

    // Tangent defaults to zero (no normal-mapping for procedural geometry by default).
    v.tangent[0] = v.tangent[1] = v.tangent[2] = 0;

    // White vertex colour / no skinning weights.
    v.color[0] = v.color[1] = v.color[2] = v.color[3] = 255;
    v.color2[0] = v.color2[1] = v.color2[2] = v.color2[3] = 0;

    vertices.push_back(v);
    return (uint16_t)(vertices.size() - 1);
}

void ModelBuilder::add_triangle(uint16_t a, uint16_t b, uint16_t c) {
    ASSERT(a < vertices.size() && b < vertices.size() && c < vertices.size()
           && "ModelBuilder::add_triangle: index out of range");
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

void ModelBuilder::add_quad(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    // Split quad into two CCW triangles: (a,b,c) and (a,c,d).
    add_triangle(a, b, c);
    add_triangle(a, c, d);
}

void ModelBuilder::begin_submesh(std::shared_ptr<MaterialInstance> mat) {
    SubMeshEntry entry{};
    entry.material    = std::move(mat);
    entry.index_start = (int)indices.size();
    submesh_entries.push_back(std::move(entry));
}

// ---------------------------------------------------------------------------
// ModelMan dynamic-model API
// ---------------------------------------------------------------------------

// Forward declaration of bounds_to_sphere defined in Model.cpp
glm::vec4 bounds_to_sphere(Bounds b);

/// Shared helper: populates m->data, m->parts, m->lods, m->materials, m->aabb,
/// and m->bounding_sphere from builder.  Does NOT upload or mark loaded.
void ModelMan::populate_model_from_builder(Model* m, const ModelBuilder& builder) {
    m->data.verts    = builder.vertices;
    m->data.indicies = builder.indices;

    // Compute AABB and bounding sphere.
    Bounds aabb;
    for (const auto& v : builder.vertices)
        aabb = bounds_union(aabb, v.pos);
    m->aabb            = aabb;
    m->bounding_sphere = bounds_to_sphere(aabb);

    const int total_verts   = (int)builder.vertices.size();
    const int total_indices = (int)builder.indices.size();

    if (builder.submesh_entries.empty()) {
        // No explicit submeshes → one submesh, engine fallback material.
        Submesh sm{};
        sm.base_vertex    = 0;
        sm.element_offset = 0;
        sm.element_count  = total_indices;
        sm.vertex_count   = total_verts;
        sm.material_idx   = 0;
        m->parts.push_back(sm);
        m->materials.push_back(imaterials->get_fallback_sptr());
    } else {
        // Build one submesh per entry, skipping any with zero indices.
        for (int i = 0; i < (int)builder.submesh_entries.size(); i++) {
            const auto& entry = builder.submesh_entries[i];
            const int index_end = (i + 1 < (int)builder.submesh_entries.size())
                                  ? builder.submesh_entries[i + 1].index_start
                                  : total_indices;
            const int count = index_end - entry.index_start;
            if (count == 0)
                continue;

            Submesh sm{};
            sm.base_vertex    = 0;
            sm.element_offset = entry.index_start * (int)sizeof(uint16_t);
            sm.element_count  = count;
            sm.vertex_count   = total_verts;
            sm.material_idx   = (int)m->materials.size();
            m->parts.push_back(sm);

            m->materials.push_back(entry.material
                                   ? entry.material
                                   : imaterials->get_fallback_sptr());
        }
    }

    // Single LOD covering all submeshes.
    MeshLod lod{};
    lod.end_percentage = 1.f;
    lod.part_ofs       = 0;
    lod.part_count     = (int)m->parts.size();
    m->lods.push_back(lod);
}

Model* ModelMan::create_dynamic_model(const ModelBuilder& builder,
                                      const std::string& debug_name) {
    ASSERT(!builder.vertices.empty() && "create_dynamic_model: builder has no vertices");
    ASSERT(!builder.indices.empty()  && "create_dynamic_model: builder has no indices");
    ASSERT(builder.indices.size() % 3 == 0
           && "create_dynamic_model: index count must be a multiple of 3");

    auto owned = std::make_unique<Model>();
    Model* m   = owned.get();

    populate_model_from_builder(m, builder);

    // Mark as dynamic so free_dynamic_model() can assert correct ownership,
    // then mark as loaded so the model is valid to pass to rendering systems.
    m->is_dynamic_model = true;
    m->init_runtime_unmanaged(debug_name);

    // Upload vertex/index data to the shared GPU buffers and register the
    // model in the all_models set (same path as asset-loaded models).
    upload_model(m);

    live_dynamic_models.push_back(std::move(owned));

    sys_print(Debug, "ModelMan: created dynamic model '%s' (%d verts, %d tris, %d submeshes)\n",
              debug_name.c_str(),
              (int)builder.vertices.size(),
              (int)builder.indices.size() / 3,
              m->get_num_parts());

    return m;
}

void ModelMan::refresh_dynamic_model(Model* m, const ModelBuilder& builder) {
    ASSERT(m && m->is_dynamic() && "refresh_dynamic_model: not a dynamic model");
    ASSERT(!builder.vertices.empty() && "refresh_dynamic_model: builder has no vertices");
    ASSERT(!builder.indices.empty()  && "refresh_dynamic_model: builder has no indices");
    ASSERT(builder.indices.size() % 3 == 0
           && "refresh_dynamic_model: index count must be a multiple of 3");

    // Free old GPU allocations and remove from the all_models set.
    remove_model_from_list(m);

    // Reset all mutable model geometry/material state before re-populating.
    m->data.verts.clear();
    m->data.indicies.clear();
    m->parts.clear();
    m->materials.clear();
    m->lods.clear();

    populate_model_from_builder(m, builder);

    // Re-register in the all_models set and re-upload to GPU.
    upload_model(m);

    sys_print(Debug, "ModelMan: refreshed dynamic model '%s' (%d verts, %d tris, %d submeshes)\n",
              m->get_name().c_str(),
              (int)builder.vertices.size(),
              (int)builder.indices.size() / 3,
              m->get_num_parts());
}

void ModelMan::free_dynamic_model(Model* m) {
    ASSERT(m && m->is_dynamic() && "free_dynamic_model: pointer is null or not a dynamic model");

    // A Render_Object proxy may still reference this Model this frame: MeshComponent::stop()'s
    // Render_Scene::remove_obj() defers the proxy's own removal until the overlapped period ends
    // (CPU work for the next frame running while the GPU still consumes the previous frame's
    // submitted proxies). Deleting the Model out from under a not-yet-removed proxy is a
    // use-after-free the next time the render scene walks proxy_list and reads proxy.model --
    // defer the same way, and flush both at the same safe point (see execute_deferred_model_frees).
    if (eng->get_is_in_overlapped_period()) {
        pending_dynamic_frees.push_back(m);
        return;
    }
    free_dynamic_model_now(m);
}

void ModelMan::free_dynamic_model_now(Model* m) {
    const std::string name = m->get_name();  // capture before deletion

    // Release GPU allocations and remove from the renderer's all_models set.
    remove_model_from_list(m);

    // Find and destroy the owning unique_ptr.
    auto it = std::find_if(live_dynamic_models.begin(), live_dynamic_models.end(),
        [m](const std::unique_ptr<Model>& ptr) { return ptr.get() == m; });
    ASSERT(it != live_dynamic_models.end() && "free_dynamic_model: model not found in live set");
    live_dynamic_models.erase(it);  // unique_ptr destructor deletes the Model

    sys_print(Debug, "ModelMan: freed dynamic model '%s' (%d remaining)\n",
              name.c_str(), (int)live_dynamic_models.size());
}

void ModelMan::execute_deferred_model_frees() {
    ASSERT(!eng->get_is_in_overlapped_period());
    for (Model* m : pending_dynamic_frees)
        free_dynamic_model_now(m);
    pending_dynamic_frees.clear();
}

// DynamicModelDeleter — defined here because it needs the full Model definition
// and access to g_modelMgr.
void DynamicModelDeleter::operator()(Model* m) const {
    ASSERT(m && m->is_dynamic()
           && "DynamicModelDeleter: pointer is null or not a dynamic model");
    g_modelMgr.free_dynamic_model(m);
}
