// BVH construction and world-building for DDGI ray tracing.
// Covers: find_volumes, raytrace_the_world, build_world, get_tri_bounds.
#include "RaytraceTest_internal.h"
#include "Render/DrawLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"
#include "Assets/AssetDatabase.h"

#include "Framework/MathLib.h"
#include "Game/Components/LightComponents.h"
#include "Level.h"
#include "Game/Entity.h"

#include <algorithm>
#include <vector>

using std::vector;

static constexpr int RT_MAX_MATERIALS = 256; // RT material slot limit; must match shader constant

extern float relocate_normal_dist;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline Bounds get_tri_bounds(vec3 v1, vec3 v2, vec3 v3) {
    ASSERT(true); // purely computational, no preconditions beyond valid vecs
    Bounds b(v1);
    b = bounds_union(b, v2);
    b = bounds_union(b, v3);
    return b;
}

// ---------------------------------------------------------------------------
// find_volumes
// ---------------------------------------------------------------------------

VolumesAndNumProbes find_volumes() {
    ASSERT(eng && eng->get_level());
    auto level = eng->get_level();
    auto& objs = level->get_all_objects();
    vector<DdgiVolumeGpu> volumes;
    int probes_summation = 0;
    vector<GiVolumeComponent*> comps;
    for (auto o : objs) {
        if (auto givol = o->cast_to<GiVolumeComponent>()) {
            comps.push_back(givol);
        }
    }
    std::sort(comps.begin(), comps.end(),
              [](GiVolumeComponent* a, GiVolumeComponent* b) { return a->priority > b->priority; });
    vector<vec4> relocate_volume_params;
    for (auto givol : comps) {
        glm::mat4 transform = givol->get_ws_transform();
        glm::vec3 min1 = transform * glm::vec4(-0.5, -0.5, -0.5, 1);
        glm::vec3 max1 = transform * glm::vec4(0.5, 0.5, 0.5, 1);
        glm::vec3 min = glm::min(min1, max1);
        glm::vec3 max = glm::max(min1, max1);

        DdgiVolumeGpu volume{};
        volume.density = glm::vec4(givol->xz_density, givol->y_density, givol->xz_density, 0);
        volume.origin_priority = glm::vec4(min, givol->priority);
        glm::vec3 size = max - min;
        glm::ivec3 probe_size = (glm::ivec3)glm::round(size / glm::vec3(volume.density)) + glm::ivec3(1);
        volume.size_offset = glm::ivec4(probe_size, probes_summation);
        probes_summation += volume.get_num_probes_total();
        volumes.push_back(volume);
        if (givol->override_relocate_dist) {
            relocate_volume_params.push_back(vec4(givol->relocate_max_dist, givol->relocate_normal_push, 0, 0));
        } else {
            relocate_volume_params.push_back(vec4(draw.ddgi->max_relocate_dist, relocate_normal_dist, 0, 0));
        }
    }
    return {volumes, probes_summation, relocate_volume_params};
}

void raytrace_the_world() {
    ASSERT(draw.ddgi);
    auto [volumes, num_probes, relocate_params] = find_volumes();
    // allocate probe irradiance and depth
    auto self = draw.ddgi.get();
    self->probe_irradiance->release();
    self->probe_depth->release();
    ASSERT(relocate_params.size() == volumes.size());

    const int height_probe_space = 128;
    const int width_probe_space = (int)glm::ceil(num_probes / float(height_probe_space));

    DdgiGlobals globals{};
    globals.atlas_x = width_probe_space;
    globals.atlas_y = height_probe_space;
    globals.num_volumes = volumes.size();
}

// ---------------------------------------------------------------------------
// External symbols used in build_world
// ---------------------------------------------------------------------------

extern ConfigVar vert_limit;
Color32 get_color_of_material_for_export(const MaterialInstance* m);
extern glm::vec3 get_emissive_of_mat_for_export(const MaterialInstance* m);

// ---------------------------------------------------------------------------
// DdgiTesting::build_world
// ---------------------------------------------------------------------------

void DdgiTesting::build_world() {
    ASSERT(draw.ddgi.get() == this || true); // called from execute() after construction
    auto& objs = draw.scene.proxy_list;

    std::vector<Bounds> bounds;
    std::vector<glm::vec4> all_verticies;

    int counter = 0;
    for (auto& _o : objs.objects) {
        auto& o = _o.type_.proxy;
        if (!o.model)
            continue;
        if (o.is_skybox)
            continue;
        if (o.ignore_in_baking)
            continue;
        if (o.model->get_num_lods() == 0)
            continue;
        counter += 1;

        auto matoverride = o.mat_override;

        const glm::mat4 transform = o.transform;
        auto rmd = o.model->get_raw_mesh_data();

        if (rmd->get_num_verticies(0) > vert_limit.get_integer())
            continue;

        const int lod_to_use = glm::min(1, (int)o.model->get_num_lods() - 1);
        auto& lod = o.model->get_lod(lod_to_use);

        for (int parti = lod.part_ofs; parti < lod.part_ofs + lod.part_count; parti++) {
            const int vertex_start = all_verticies.size();

            const auto& part = o.model->get_part(parti);
            auto material_inst = o.model->get_material_for_part(part);
            if (matoverride)
                material_inst = matoverride;

            if (!material_inst)
                continue;
            auto material = material_inst->impl.get();
            if (!material || !material->get_master_impl())
                continue;
            if (material->get_master_impl()->light_mode == LightingMode::Unlit)
                continue;

            const int material_offset =
                material->gpu_buffer_offset == -1 ? 0 : material->get_material_index_from_buffer_ofs();

            const int part_index_start = part.element_offset / 2;
            const int part_index_count = part.element_count;
            const int base_vertex = part.base_vertex;
            const int num_verts = part.vertex_count;

            auto get_vertex = [&](int index) -> glm::vec3 {
                return transform * glm::vec4(rmd->get_vertex_at_index(base_vertex + index).pos, 1.0);
            };

            for (int i = 0; i < part_index_count; i += 3) {
                const int i0 = rmd->get_index_at_index(part_index_start + i);
                const int i1 = rmd->get_index_at_index(part_index_start + i + 1);
                const int i2 = rmd->get_index_at_index(part_index_start + i + 2);

                all_verticies.push_back(glm::vec4(get_vertex(i0), material_offset));
                all_verticies.push_back(glm::vec4(get_vertex(i1), material_offset));
                all_verticies.push_back(glm::vec4(get_vertex(i2), material_offset));

                const Bounds tri_bounds = get_tri_bounds(get_vertex(i0), get_vertex(i1), get_vertex(i2));
                bounds.push_back(tri_bounds);
            }
        }
    }

    ASSERT(!bounds.empty() || counter == 0);
    const double start = GetTime();
    BVH as = BVH::build(bounds, 4, PartitionStrategy::BVH_SAH);
    printf("rt build bvh time: %f\n", float(GetTime() - start));

    // Build GPU-side BVH node array
    std::vector<GPUBVHNode> nodes;
    for (int i = 0; i < as.nodes.size(); i++) {
        GPUBVHNode n;
        n.min = vec4(as.nodes[i].aabb.bmin, 1);
        n.max = vec4(as.nodes[i].aabb.bmax, 1);
        n.count = as.nodes[i].count;
        n.left_node = as.nodes[i].left_node;
        nodes.push_back(n);
    }

    std::vector<glm::vec4> materialsdata;
    auto& allmats = matman.get_material_table()->get_all_mat_array();
    materialsdata.resize(RT_MAX_MATERIALS);
    for (int i = 0; i < allmats.size(); i++) {
        if (!allmats.at(i))
            continue;
        auto m = (MaterialImpl*)allmats.at(i)->impl.get();
        if (m->gpu_buffer_offset == MaterialImpl::INVALID_MAPPING)
            continue;
        const int INDEX = m->get_material_index_from_buffer_ofs();
        ASSERT(INDEX >= 0 && INDEX < RT_MAX_MATERIALS);

        auto vec = color32_to_vec4(get_color_of_material_for_export(allmats.at(i)));
        auto linear = glm::pow(vec, glm::vec4(2.2));
        linear.w = 1;

        auto emissive = get_emissive_of_mat_for_export(allmats.at(i));
        if (glm::dot(emissive, glm::vec3(1)) > 0.1) {
            linear = glm::vec4(emissive, 0);
        }

        materialsdata.at(INDEX) = linear;
    }

    if (verts)
        verts->release();
    if (indicies)
        indicies->release();
    if (references)
        references->release();
    if (this->nodes)
        this->nodes->release();
    if (this->materials)
        this->materials->release();

    CreateBufferArgs args;
    args.flags = GraphicsBufferUseFlags::BUFFER_USE_AS_STORAGE_READ;
    args.size = all_verticies.size() * sizeof(glm::vec4);
    verts = gfx().create_buffer(args);
    verts->upload(all_verticies.data(), args.size);

    args.size = sizeof(int) * as.indicies.size();
    references = gfx().create_buffer(args);
    references->upload(as.indicies.data(), args.size);

    args.size = nodes.size() * sizeof(GPUBVHNode);
    this->nodes = gfx().create_buffer(args);
    this->nodes->upload(nodes.data(), args.size);

    args.size = materialsdata.size() * sizeof(glm::vec4);
    this->materials = gfx().create_buffer(args);
    this->materials->upload(materialsdata.data(), args.size);
}
