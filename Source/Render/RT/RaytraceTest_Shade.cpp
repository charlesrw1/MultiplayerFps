// DDGI shading, lighting passes, probe debug rendering, render_rt.
// Covers: render_probes, draw_lighting*, set_reflection_uniforms, render_rt.
#include "RaytraceTest.h"
#include "Render/DrawLocal.h"
#include "Render/ModelManager.h"
#include "Render/RenderGiManager.h"

#include "Framework/MathLib.h"

void set_shit_fuck();
void draw_model_simple_no_material(Model* model);

// Statics owned by RaytraceTest.cpp
extern float irrad_mult;
extern float normal_bias;
extern float view_bias;
extern float depth_sigma;
extern float normal_sigma;
extern vec2 ssr_lum_range;
extern int lum_adjust_mode;
extern ConfigVar include_cubemaps;
extern ConfigVar r_ddgi_halfres;
extern ConfigVar r_ddgi_halfres_blend;

// ---------------------------------------------------------------------------
// Probe debug rendering
// ---------------------------------------------------------------------------

ConfigVar draw_real_grid("draw_real_grid", "2", CVAR_INTEGER | CVAR_UNBOUNDED, "");

void DdgiTesting::render_probes() {
    ASSERT(true); // myvolumes may be empty — guard is at top of function
    if (myvolumes.size() == 0)
        return;

    auto& device = draw.get_device();

    auto set_composite_pass = [&]() {
        RenderPassState pass_state;
        auto color_infos = {ColorTargetInfo(draw.tex.output_composite)}; // no clear
        pass_state.color_infos = color_infos;
        pass_state.depth_info = draw.tex.scene_depth;
        gfx().set_render_pass(pass_state);
    };
    set_composite_pass();

    RenderPipelineState state = RenderPipelineState();
    state.program = draw.get_prog_man().get_obj(debug_probes);
    state.vao = g_modelMgr.get_vao_ptr(VaoType::Animated);
    device.set_pipeline(state);

    Model* m = Model::load("sphere.cmdl");

    device.bind_texture(0, probe_irradiance);
    set_shit_fuck();
    if (draw_real_grid.get_integer() == 2) {
        for (auto& vol : myvolumes) {
            auto ddgiGRID = vol.size_offset;
            gpu::MeshDebugProbeFragPushConsts pcf{};
            pcf.vol_grid   = glm::ivec4(glm::ivec3(ddgiGRID), 0);
            pcf.vol_offset = ddgiGRID.w;

            for (int x = 0; x < ddgiGRID.x; x++) {
                for (int y = 0; y < ddgiGRID.y; y++) {
                    for (int z = 0; z < ddgiGRID.z; z++) {

                        const int global_index = x + y * ddgiGRID.x + z * ddgiGRID.x * ddgiGRID.y + ddgiGRID.w;
                        glm::vec3 ofs = glm::vec3(temp_probe_relocate_thing.at(global_index));

                        glm::mat4 tr = glm::translate(glm::mat4(1), glm::vec3(x, y, z) * glm::vec3(vol.density) +
                                                                        glm::vec3(vol.origin_priority) + ofs);
                        gpu::MeshSimpleVertPushConsts pcv{};
                        pcv.Model = glm::scale(tr, glm::vec3(0.2));
                        gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

                        pcf.probe_coord = glm::ivec4(x, y, z, 0);
                        gfx().push_fragment_constants(0, &pcf, sizeof(pcf));

                        draw_model_simple_no_material(m);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Reflection / cubemap uniforms
// ---------------------------------------------------------------------------

void DdgiTesting::fill_reflection_params(gpu::DdgiRuntimeParams& out_params) {
    ASSERT(RenderGiManager::inst);

    extern ConfigVar r_specular_ao_intensity;
    out_params.specular_ao_intensity = r_specular_ao_intensity.get_float();
    out_params.lum_adjust_mode = lum_adjust_mode;

    IGraphicsTexture* const cubemap_array = RenderGiManager::inst->get_cubemap_array_texture();
    const int num_cubemaps = RenderGiManager::inst->get_num_cubemaps();
    IGraphicsBuffer* const cubemap_volume_buffer = RenderGiManager::inst->get_cubemap_volume_buffer();

    gfx().bind_storage_buffer_base(11, cubemap_volume_buffer);
    draw.bind_texture_ptr(8, cubemap_array);
    out_params.num_cubemaps = num_cubemaps;

    draw.bind_texture_ptr(7, EnviornmentMapHelper::get().integrator.get_texture());
    draw.bind_texture_ptr(9, draw.scene.skylights.at(0).skylight.generated_cube->gpu_ptr);
}

// ---------------------------------------------------------------------------
// Shared shading uniforms
// ---------------------------------------------------------------------------

void DdgiTesting::draw_lighting_shared(IGraphicsTexture* ssao, bool for_cubemap_view, gpu::DdgiRuntimeParams& out_params) {
    ASSERT(ssao);

    theglobals.view_bias = view_bias;
    theglobals.normal_bias = normal_bias;
    ddgi_globals->upload(&theglobals, sizeof(theglobals));

    draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
    draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
    draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
    draw.bind_texture_ptr(3, draw.tex.scene_depth);

    draw.bind_texture_ptr(4, probe_irradiance);
    draw.bind_texture_ptr(5, probe_depth);
    draw.bind_texture_ptr(6, ssao);

    out_params.include_cubemaps = (!for_cubemap_view && include_cubemaps.get_bool()) ? 1 : 0;

    set_shit_fuck();

    out_params.selected_probe_pos = glm::ivec4(selected_probe, 0);
    out_params.irrad_mult = irrad_mult;
    fill_reflection_params(out_params);
}

// ---------------------------------------------------------------------------
// Full-resolution lighting pass
// ---------------------------------------------------------------------------

void DdgiTesting::draw_lighting_fullres(IGraphicsTexture* ssao, bool for_cubemap_view) {
    ASSERT(ssao && shade_fs != -1);
    auto& device = draw.get_device();

    RenderPipelineState state;
    state.vao = draw.get_empty_vao();
    state.program = draw.get_prog_man().get_obj(shade_fs);
    state.blend = BlendState::ADD;
    state.depth_testing = false;
    state.depth_writes = false;
    device.set_pipeline(state);

    gpu::DdgiRuntimeParams dp{};
    draw_lighting_shared(ssao, for_cubemap_view, dp);
    draw.ubo.ddgi_runtime_params->upload(&dp, sizeof(dp));
    gfx().bind_uniform_buffer_base(7, draw.ubo.ddgi_runtime_params);

    draw.bind_texture_ptr(10, draw.tex.reflection_accum);

    gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

// ---------------------------------------------------------------------------
// Half-resolution lighting + temporal upsample pass
// ---------------------------------------------------------------------------

ConfigVar r_ddgi_halfres_blend("r.ddgi_halfres_blend", "0.8", CVAR_FLOAT,
                               "[0,1] blend input into ddgi temporal accumulation");

void DdgiTesting::draw_lighting_halfres(IGraphicsTexture* ssao) {
    ASSERT(ssao && shade_fs_halfres != -1);
    auto& device = draw.get_device();

    const glm::ivec2 texel_offset = get_half_res_offset();

    auto targets = {ColorTargetInfo(draw.tex.halfres_scene_color)};
    RenderPassState rp;
    rp.color_infos = targets;
    gfx().set_render_pass(rp);

    RenderPipelineState state;
    state.vao = draw.get_empty_vao();
    state.program = draw.get_prog_man().get_obj(shade_fs_halfres);
    state.blend = BlendState::OPAQUE;
    state.depth_testing = false;
    state.depth_writes = false;
    device.set_pipeline(state);

    gpu::DdgiRuntimeParams dp{};
    draw_lighting_shared(ssao, false, dp);
    dp.halfres_texel_offset = glm::ivec2(texel_offset.x, texel_offset.y);
    draw.ubo.ddgi_runtime_params->upload(&dp, sizeof(dp));
    gfx().bind_uniform_buffer_base(7, draw.ubo.ddgi_runtime_params);

    gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

    {
        GPUSCOPESTART(temporal_upsample_ddgi);

        auto targets2 = {ColorTargetInfo(draw.tex.ddgi_accum)};
        rp.color_infos = targets2;
        gfx().set_render_pass(rp);

        state.blend = BlendState::OPAQUE;
        state.program = draw.get_prog_man().get_obj(temporal_upsample);
        device.set_pipeline(state);

        draw.bind_texture_ptr(0, draw.tex.halfres_scene_color);
        draw.bind_texture_ptr(1, draw.tex.last_ddgi_accum);
        draw.bind_texture_ptr(2, draw.tex.scene_depth);
        draw.bind_texture_ptr(3, draw.tex.scene_motion);
        draw.bind_texture_ptr(4, draw.tex.last_scene_motion);

        // FIXME
        extern ConfigVar r_taa_blend;
        extern ConfigVar r_taa_flicker_remove;
        extern ConfigVar r_taa_reproject;
        extern ConfigVar r_taa_dilate_velocity;
        extern float taa_doc_mult;
        extern float taa_doc_vel_bias;
        extern float taa_doc_bias;
        extern float taa_doc_pow;

        gpu::TemporalParams tp{};
        tp.lastViewProj = draw.last_frame_main_view.viewproj;
        tp.amt = r_ddgi_halfres_blend.get_float();
        tp.doc_mult = taa_doc_mult;
        tp.doc_vel_bias = taa_doc_vel_bias;
        tp.doc_bias = taa_doc_bias;
        tp.doc_pow = taa_doc_pow;
        tp.remove_flicker = r_taa_flicker_remove.get_bool();
        tp.use_reproject = r_taa_reproject.get_bool();
        tp.dilate_velocity = r_taa_dilate_velocity.get_bool();
        tp.halfres_texel_offset = glm::ivec2(texel_offset.x, texel_offset.y);
        draw.ubo.temporal_params->upload(&tp, sizeof(tp));
        gfx().bind_uniform_buffer_base(7, draw.ubo.temporal_params);

        gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
    }

    GPUSCOPESTART(ddgihalfres_apply);

    // Apply accumulated half-res ddgi to scene
    auto targets3 = {ColorTargetInfo(draw.tex.scene_color)};
    rp.color_infos = targets3;
    gfx().set_render_pass(rp);
    state.program = draw.get_prog_man().get_obj(apply_halfres_accum_to_scene);
    state.blend = BlendState::ADD;
    device.set_pipeline(state);
    draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
    draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
    draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
    draw.bind_texture_ptr(3, draw.tex.scene_depth);
    draw.bind_texture_ptr(4, ssao);
    draw.bind_texture_ptr(5, draw.tex.ddgi_accum);
    draw.bind_texture_ptr(6, draw.tex.reflection_accum);

    {
        gpu::DdgiRuntimeParams dp_apply{};
        dp_apply.ssr_lum_range = ssr_lum_range;
        fill_reflection_params(dp_apply);
        draw.ubo.ddgi_runtime_params->upload(&dp_apply, sizeof(dp_apply));
        gfx().bind_uniform_buffer_base(7, draw.ubo.ddgi_runtime_params);
    }

    gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

    std::swap(draw.tex.ddgi_accum, draw.tex.last_ddgi_accum);
    increment_half_res_offset_index();
}

// ---------------------------------------------------------------------------
// Top-level lighting entry point
// ---------------------------------------------------------------------------

void DdgiTesting::draw_lighting(IGraphicsTexture* ssao, bool for_cubemap_view) {
    ASSERT(ssao);
    if (draw.scene.skylights.empty())
        return; // fixme

    auto& device = draw.get_device();

    GPUFUNCTIONSTART;

    if (r_ddgi_halfres.get_bool() && !for_cubemap_view)
        draw_lighting_halfres(ssao);
    else
        draw_lighting_fullres(ssao, for_cubemap_view);
}

// ---------------------------------------------------------------------------
// Debug: ray-test composite render
// ---------------------------------------------------------------------------

void DdgiTesting::render_rt() {
    ASSERT(raytrace_test != -1);
    GPUFUNCTIONSTART;

    if (!verts) {
        build_world();
        ASSERT(verts);
    }
    auto& device = draw.get_device();
    auto set_composite_pass = [&]() {
        RenderPassState pass_state;
        ColorTargetInfo target(draw.tex.output_composite);
        target.wants_clear = true; // clear to black (default)
        auto color_infos = {target};
        pass_state.color_infos = color_infos;
        gfx().set_render_pass(pass_state);
    };
    set_composite_pass();

    RenderPipelineState state;
    state.program = draw.get_prog_man().get_obj(raytrace_test);
    state.vao = draw.get_empty_vao();
    device.set_pipeline(state);

    gfx().bind_storage_buffer_base(3, verts);
    gfx().bind_storage_buffer_base(5, nodes);
    gfx().bind_storage_buffer_base(6, references);
    gfx().bind_storage_buffer_base(7, materials);

    gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}
