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
        pass_state.wants_color_clear = false;
        auto color_infos = {ColorTargetInfo(draw.tex.output_composite)};
        pass_state.color_infos = color_infos;
        pass_state.depth_info = draw.tex.scene_depth;
        IGraphicsDevice::inst->set_render_pass(pass_state);
    };
    set_composite_pass();

    RenderPipelineState state = RenderPipelineState();
    state.program = debug_probes;
    state.vao = g_modelMgr.get_vao_ptr(VaoType::Animated)->get_internal_handle();
    device.set_pipeline(state);

    Model* m = Model::load("sphere.cmdl");

    device.bind_texture_ptr(0, probe_irradiance);
    set_shit_fuck();
    if (draw_real_grid.get_integer() == 2) {
        for (auto& vol : myvolumes) {
            auto ddgiGRID = vol.size_offset;
            device.shader().set_ivec3("vol_grid", ddgiGRID);
            device.shader().set_int("vol_offset", ddgiGRID.w);

            for (int x = 0; x < ddgiGRID.x; x++) {
                for (int y = 0; y < ddgiGRID.y; y++) {
                    for (int z = 0; z < ddgiGRID.z; z++) {

                        const int global_index = x + y * ddgiGRID.x + z * ddgiGRID.x * ddgiGRID.y + ddgiGRID.w;
                        glm::vec3 ofs = glm::vec3(temp_probe_relocate_thing.at(global_index));

                        glm::mat4 tr = glm::translate(glm::mat4(1), glm::vec3(x, y, z) * glm::vec3(vol.density) +
                                                                        glm::vec3(vol.origin_priority) + ofs);
                        device.shader().set_mat4("Model", glm::scale(tr, glm::vec3(0.2)));
                        device.shader().set_ivec3("probe_coord", {x, y, z});

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

void DdgiTesting::set_reflection_uniforms() {
    ASSERT(RenderGiManager::inst);
    auto& device = draw.get_device();

    extern ConfigVar r_specular_ao_intensity;
    device.shader().set_float("specular_ao_intensity", r_specular_ao_intensity.get_float());
    device.shader().set_int("lum_adjust_mode", lum_adjust_mode);

    {
        IGraphicsTexture* const cubemap_array = RenderGiManager::inst->get_cubemap_array_texture();
        const int num_cubemaps = RenderGiManager::inst->get_num_cubemaps();
        IGraphicsBuffer* const cubemap_volume_buffer = RenderGiManager::inst->get_cubemap_volume_buffer();

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, cubemap_volume_buffer->get_internal_handle());
        draw.bind_texture_ptr(8, cubemap_array);
        device.shader().set_int("num_cubemaps", num_cubemaps);
    }

    draw.bind_texture(7, EnviornmentMapHelper::get().integrator.get_texture());
    draw.bind_texture(9, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());
}

// ---------------------------------------------------------------------------
// Shared shading uniforms
// ---------------------------------------------------------------------------

void DdgiTesting::draw_lighting_shared(IGraphicsTexture* ssao, bool for_cubemap_view) {
    ASSERT(ssao);
    auto& device = draw.get_device();

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

    extern ConfigVar r_specular_ao_intensity;
    device.shader().set_bool("include_cubemaps", !for_cubemap_view && include_cubemaps.get_bool());

    set_shit_fuck();

    device.shader().set_ivec3("selected_probe_pos", selected_probe);
    device.shader().set_float("irrad_mult", irrad_mult);
    device.shader().set_bool("wants_half_res", false);
    set_reflection_uniforms();
}

// ---------------------------------------------------------------------------
// Full-resolution lighting pass
// ---------------------------------------------------------------------------

void DdgiTesting::draw_lighting_fullres(IGraphicsTexture* ssao, bool for_cubemap_view) {
    ASSERT(ssao && shade_fs != -1);
    auto& device = draw.get_device();

    RenderPipelineState state;
    state.vao = draw.get_empty_vao();
    state.program = shade_fs;
    state.blend = BlendState::ADD;
    state.depth_testing = false;
    state.depth_writes = false;
    device.set_pipeline(state);

    draw_lighting_shared(ssao, for_cubemap_view);

    glDrawArrays(GL_TRIANGLES, 0, 3);
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
    IGraphicsDevice::inst->set_render_pass(rp);

    RenderPipelineState state;
    state.vao = draw.get_empty_vao();
    state.program = shade_fs_halfres;
    state.blend = BlendState::OPAQUE;
    state.depth_testing = false;
    state.depth_writes = false;
    device.set_pipeline(state);

    draw_lighting_shared(ssao, false);
    device.shader().set_ivec2("halfres_texel_offset", texel_offset);
    device.shader().set_bool("wants_half_res", true);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    {
        GPUSCOPESTART(temporal_upsample_ddgi);

        auto targets2 = {ColorTargetInfo(draw.tex.ddgi_accum)};
        rp.color_infos = targets2;
        IGraphicsDevice::inst->set_render_pass(rp);

        state.blend = BlendState::OPAQUE;
        state.program = temporal_upsample;
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

        auto the_shader = device.shader();
        the_shader.set_float("amt", r_ddgi_halfres_blend.get_float());
        the_shader.set_bool("remove_flicker", r_taa_flicker_remove.get_bool());
        the_shader.set_mat4("lastViewProj", draw.last_frame_main_view.viewproj);
        the_shader.set_bool("use_reproject", r_taa_reproject.get_bool());
        the_shader.set_float("doc_mult", taa_doc_mult);
        the_shader.set_float("doc_vel_bias", taa_doc_vel_bias);
        the_shader.set_float("doc_bias", taa_doc_bias);
        the_shader.set_float("doc_pow", taa_doc_pow);
        the_shader.set_bool("dilate_velocity", r_taa_dilate_velocity.get_bool());
        the_shader.set_ivec2("halfres_texel_offset", texel_offset);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    GPUSCOPESTART(ddgihalfres_apply);

    // Apply accumulated half-res ddgi to scene
    auto targets3 = {ColorTargetInfo(draw.tex.scene_color)};
    rp.color_infos = targets3;
    IGraphicsDevice::inst->set_render_pass(rp);
    state.program = apply_halfres_accum_to_scene;
    state.blend = BlendState::ADD;
    device.set_pipeline(state);
    draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
    draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
    draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
    draw.bind_texture_ptr(3, draw.tex.scene_depth);
    draw.bind_texture_ptr(4, ssao);
    draw.bind_texture_ptr(5, draw.tex.ddgi_accum);
    device.shader().set_vec2("ssr_lum_range", ssr_lum_range);
    extern ConfigVar enable_ssr;
    if (enable_ssr.get_bool())
        draw.bind_texture_ptr(6, draw.tex.reflection_accum);
    else
        draw.bind_texture_ptr(6, draw.black_texture);

    set_reflection_uniforms();

    glDrawArrays(GL_TRIANGLES, 0, 3);

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
        pass_state.wants_color_clear = true;
        auto color_infos = {ColorTargetInfo(draw.tex.output_composite)};
        pass_state.color_infos = color_infos;
        IGraphicsDevice::inst->set_render_pass(pass_state);
    };
    set_composite_pass();

    RenderPipelineState state;
    state.program = raytrace_test;
    state.vao = draw.get_empty_vao();
    device.set_pipeline(state);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verts->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());

    glDrawArrays(GL_TRIANGLES, 0, 3);
}
