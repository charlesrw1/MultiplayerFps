// DDGI probe execution: trace, gather, relocate, texture allocation, lum calc.
// Covers: compute_avg_probe_value, create_textures_raybuffer, execute, calculate_lum_for_spec.
#include "RaytraceTest_internal.h"
#include "Render/DrawLocal.h"
#include "Render/RenderGiManager.h"

#include "Framework/MathLib.h"

// Statics defined in RaytraceTest.cpp, shared via extern.
extern float relocate_normal_dist;
extern int bounces;
extern bool do_flush_after;
extern bool skip_gather;

void set_shit_fuck();


// ---------------------------------------------------------------------------
// Texture atlas allocation
// ---------------------------------------------------------------------------

void DdgiTesting::create_textures_raybuffer(int probe_width, int probe_height) {
    ASSERT(probe_width > 0 && probe_height > 0);

    if (this->probe_depth)
        this->probe_depth->release();
    if (this->probe_irradiance)
        this->probe_irradiance->release();

    CreateTextureArgs targs;
    const int tiles_wide = probe_width;
    const int tiles_height = probe_height;
    targs.width = tiles_wide * ddgiIRRADTILE;
    targs.height = tiles_height * ddgiIRRADTILE;

    targs.type = GraphicsTextureType::t2D;
    targs.num_mip_maps = 1;
    targs.format = GraphicsTextureFormat::r11f_g11f_b10f;
    targs.sampler_type = GraphicsSamplerType::LinearNoMipmaps;
    probe_irradiance = IGraphicsDevice::inst->create_texture(targs);
    probe_irradiance->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);

    auto handle = Texture::load("_ddgi");
    handle->update_specs_ptr(this->probe_irradiance);

    targs.width = tiles_wide * ddgiDEPTHTILE;
    targs.height = tiles_height * ddgiDEPTHTILE;
    targs.format = GraphicsTextureFormat::rg16f;
    probe_depth = IGraphicsDevice::inst->create_texture(targs);
    probe_depth->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);

    handle = Texture::load("_ddgi_d");
    handle->update_specs_ptr(this->probe_depth);
}

// ---------------------------------------------------------------------------
// Average probe luminance
// ---------------------------------------------------------------------------

void DdgiTesting::compute_avg_probe_value() {
    ASSERT(ddgi_probe_avg_value);
    const int num_probes = temp_probe_relocate_thing.size();
    ddgi_probe_avg_value->upload(nullptr, num_probes * sizeof(float) * 3);

    if (!probe_depth || !probe_irradiance)
        return;
    auto& device = draw.get_device();
    device.bind_texture(2, probe_irradiance->get_internal_handle());
    device.bind_texture(3, probe_depth->get_internal_handle());

    device.set_shader(avg_probe_calc);
    set_shit_fuck();

    const int groups = glm::ceil(num_probes / 64.f);
    printf("compute_avg_probe_value %d\n", groups);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, ddgi_probe_avg_value->get_internal_handle());
    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// ---------------------------------------------------------------------------
// Main execute — relocate + trace + gather loop
// ---------------------------------------------------------------------------

ConfigVar r_rt_skiprebuild("r.rt_skiprebuild", "0", CVAR_BOOL, "");

void DdgiTesting::execute() {
    ASSERT(ddgi_globals && ddgi_volumes);
    if (!verts || !r_rt_skiprebuild.get_bool())
        build_world();
    double start = GetTime();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verts->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());

    auto [volumes, total_num_probes, relocate_params] = find_volumes();
    if (volumes.size() == 0)
        throw 1;
    ASSERT(relocate_params.size() == volumes.size());

    this->myvolumes = volumes;

    const int height_probe_space = 128;
    const int width_probe_space = (int)glm::ceil(total_num_probes / float(height_probe_space));

    DdgiGlobals globals{};
    globals.atlas_x = width_probe_space;
    globals.atlas_y = height_probe_space;
    globals.num_volumes = volumes.size();
    globals.max_relocate_dist = max_relocate_dist;
    globals.indirect_boost = indirect_boost;
    globals.relocate_normal_dist = relocate_normal_dist;
    ddgi_globals->upload(&globals, sizeof(globals));
    ddgi_volumes->upload(volumes.data(), volumes.size() * sizeof(DdgiVolumeGpu));
    theglobals = globals;

    ddgi_probe_relocation_offsets->upload(nullptr, sizeof(glm::vec4) * total_num_probes);

    CreateBufferArgs args{};
    args.size = total_num_probes * MAX_RAYS * sizeof(RayBufferStruct);
    IGraphicsBuffer* ray_buffer = IGraphicsDevice::inst->create_buffer(args);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ray_buffer->get_internal_handle());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, draw.buf.lighting_uniforms->get_internal_handle());

    create_textures_raybuffer(width_probe_space, height_probe_space);

    auto& device = draw.get_device();

    IGraphicsBuffer* invalid_count_buf{};
    invalid_count_buf = IGraphicsDevice::inst->create_buffer({});
    glm::ivec4 counter_num = {};
    invalid_count_buf->upload(&counter_num, sizeof(glm::ivec4));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, invalid_count_buf->get_internal_handle());

    // Probe relocation pass
    Random r(13);
    {
        device.set_shader(relocate_shader);

        IGraphicsBuffer* relocate_param_buf = IGraphicsDevice::inst->create_buffer({});
        relocate_param_buf->upload(relocate_params.data(), relocate_params.size() * sizeof(vec4));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, relocate_param_buf->get_internal_handle());

        set_shit_fuck();
        device.shader().set_float("ray_sample_randomness", r.RandF(0, TWOPI));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, ddgi_probe_relocation_offsets->get_internal_handle());
        const int total_probes = total_num_probes;
        const int groups = glm::ceil(total_probes / 64.f);
        glDispatchCompute(groups, 1, 1);

        {
            temp_probe_relocate_thing.resize(total_num_probes);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ddgi_probe_relocation_offsets->get_internal_handle());
            glm::vec4* ptr = (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
            for (int i = 0; i < total_num_probes; i++)
                temp_probe_relocate_thing.at(i) = ptr[i];
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        relocate_param_buf->release();
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Trace + gather loop
    for (int i = 0; i < bounces; i++) {
        device.bind_texture(5, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());

        device.bind_texture(2, probe_irradiance->get_internal_handle());
        device.bind_texture(3, probe_depth->get_internal_handle());

        device.set_shader(trace_shader);
        device.shader().set_bool("do_irrad_calcs", i != 0);
        set_shit_fuck();
        device.shader().set_float("ray_sample_randomness", r.RandF(0, TWOPI));
        device.shader().set_int("num_lights", draw.scene.light_list.objects.size());

        if (draw.scene.suns.size() > 0) {
            auto& sun = draw.scene.suns.at(0).sun;
            device.shader().set_vec4("light_sun_dir", vec4(sun.direction, 1));
            device.shader().set_vec4("light_sun_color", vec4(sun.color, 0));
        } else {
            device.shader().set_vec4("light_sun_dir", vec4(0, 0, 0, -1));
        }

        const int total_probes = total_num_probes;
        const int groups = total_probes;

        printf("trace %d\n", i);
        glDispatchCompute(groups, 1, 1);

        if (!skip_gather) {
            device.set_shader(gather_shader);
            set_shit_fuck();
            device.shader().set_int("num_runs_so_far", glm::max(0, i - 1));

            glBindImageTexture(0, probe_irradiance->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                               GL_R11F_G11F_B10F);

            glBindImageTexture(1, probe_depth->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

            glCheckError();
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            printf("gather %d\n", i);

            const int gather_groups = total_probes;
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14,
                             ddgi_probe_relocation_offsets->get_internal_handle());

            glDispatchCompute(gather_groups, 1, 1);
            glCheckError();

            if (i == 0) {
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, invalid_count_buf->get_internal_handle());
                glm::ivec4* ptr = (glm::ivec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
                glm::ivec4 result = *ptr;
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                printf("total_probes %d\n", total_probes);
                printf("invalid probes: %d\n", result.y);
                printf("no_depth_needed probes: %d\n", result.x);
            }
        }

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    if (do_flush_after)
        glFinish();
    float time = GetTime() - start;
    sys_print(Debug, "sfasdf asdf sadftime: %f\n", time);

    compute_avg_probe_value();

    ray_buffer->release();
    invalid_count_buf->release();
}

// ---------------------------------------------------------------------------
// Cubemap luminance pre-calculation
// ---------------------------------------------------------------------------

void DdgiTesting::calculate_lum_for_spec() {
    ASSERT(lum_calc != -1);
    const int num = RenderGiManager::inst->get_num_cubemaps();
    if (num == 0)
        return;
    if (!probe_depth || !probe_irradiance)
        return;
    auto& device = draw.get_device();
    device.bind_texture(2, probe_irradiance->get_internal_handle());
    device.bind_texture(3, probe_depth->get_internal_handle());

    device.set_shader(lum_calc);
    set_shit_fuck();
    const int num_cubemaps = RenderGiManager::inst->get_num_cubemaps();
    device.shader().set_int("num_cubemaps", num_cubemaps);

    const int groups = glm::ceil(num / 64.f);
    printf("$$$$$$$$$$$$$$$$$$calculate_lum_for_spec %d\n", groups);

    IGraphicsBuffer* const cubemap_volume_buffer = RenderGiManager::inst->get_cubemap_volume_buffer();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, cubemap_volume_buffer->get_internal_handle());
    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
