// DDGI ray-tracing system: constructor, destructor, debug menu, set_shit_fuck, load_the_gi.
// Heavy logic split into:
//   RaytraceTest_BVH.cpp   — scene BVH build
//   RaytraceTest_Probe.cpp — probe trace/gather/execute
//   RaytraceTest_Shade.cpp — shading/lighting passes
#include "RaytraceTest.h"
#include "RaytraceTest_internal.h"
#include "Render/DrawLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"

#include "Framework/MathLib.h"

#include "imgui.h"

// MAX_RAYS, ddgiIRRADTILE, ddgiDEPTHTILE defined in RaytraceTest_internal.h (included by split files)

using std::vector;

// ---------------------------------------------------------------------------
// Shared statics referenced by other translation units via extern
// ---------------------------------------------------------------------------

float irrad_mult = 1.0f;
float normal_bias = 0.2f;
float view_bias = 0.3f;
int bounces = 4;
float relocate_normal_dist = 0.2f;
int lum_adjust_mode = 4;
float depth_sigma = 50.0f;
float normal_sigma = 1.0f;
bool do_flush_after = true;
bool skip_gather = false;
vec2 ssr_lum_range = vec2(0, 0.1f);

ConfigVar include_cubemaps("r.include_cubemaps", "1", CVAR_BOOL, "");
ConfigVar vert_limit("vert_limit", "9999999", CVAR_INTEGER | CVAR_UNBOUNDED, "");
ConfigVar r_ddgi_halfres("r.ddgi_halfres", "1", CVAR_BOOL, "");

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DdgiTesting::DdgiTesting() {
    ASSERT(gfx_is_initialized());
    raytrace_test = draw.get_prog_man().create_raster("fullscreenquad.txt", "rtF.txt");

    gather_shader = draw.get_prog_man().create_compute("gather_C.txt");
    shade_fs = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeF.txt");
    shade_fs_halfres = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeF.txt", "HALFRES_DDGI");

    trace_shader = draw.get_prog_man().create_compute("trace_C.txt");
    debug_probes = draw.get_prog_man().create_raster("MeshSimpleV.txt", "MeshDebugProbeF.txt");

    relocate_shader = draw.get_prog_man().create_compute("trace_C.txt", "RELOCATE");

    lum_calc = draw.get_prog_man().create_compute("probeLumCalc_C.txt");

    avg_probe_calc = draw.get_prog_man().create_compute("avgProbeCalc_C.txt");

    temporal_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "temporal_upsample_ddgi.txt");
    apply_halfres_accum_to_scene = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgi_apply_upsampled.txt");

    ddgi_probe_relocation_offsets = gfx().create_buffer({});
    ddgi_globals = gfx().create_buffer({});
    ddgi_volumes = gfx().create_buffer({});

    ddgi_probe_avg_value = gfx().create_buffer({});

    Texture::install_system("_ddgi");
    Texture::install_system("_ddgi_d");
}

DdgiTesting::~DdgiTesting() {}

// ---------------------------------------------------------------------------
// GPU binding helper — sets DDGI uniform/storage buffer bindings
// ---------------------------------------------------------------------------

void set_shit_fuck() {
    ASSERT(draw.ddgi);
    auto self = draw.ddgi.get();

    gfx().bind_uniform_buffer_base(8, self->ddgi_globals);
    gfx().bind_storage_buffer_base(9, self->ddgi_volumes);
    gfx().bind_storage_buffer_base(12, self->ddgi_probe_relocation_offsets);
}

// ---------------------------------------------------------------------------
// Debug menu
// ---------------------------------------------------------------------------

extern void draw_imgui_for_cvar(ConfigVar& var);

void ddgi_debugmenu() {
    auto self = draw.ddgi.get();
    ImGui::InputInt3("select", &self->selected_probe.x);

    ImGui::DragFloat("irradmul", &irrad_mult, 0.01);

    if (ImGui::Button("refresh"))
        self->execute();
    ImGui::InputFloat("normal bias", &normal_bias);
    ImGui::InputFloat("view bias", &view_bias);
    ImGui::InputInt("bounces", &bounces);
    draw_imgui_for_cvar(include_cubemaps);

    ImGui::InputFloat("relocate_normal_dist", &relocate_normal_dist);
    ImGui::SliderInt("lum_adjust", &lum_adjust_mode, 0, 7);

    ImGui::InputFloat("max_relocate_dist", &self->max_relocate_dist);
    ImGui::InputFloat("indirect_boost", &self->indirect_boost);

    ImGui::InputFloat("depth_sigma", &depth_sigma);
    ImGui::InputFloat("normal_sigma", &normal_sigma);

    draw_imgui_for_cvar(r_ddgi_halfres);
    ImGui::Checkbox("do_flush", &do_flush_after);
    ImGui::Checkbox("skip_gather", &skip_gather);
    ImGui::InputFloat("ssr_lum_min", &ssr_lum_range.x);
    ImGui::InputFloat("ssr_lum_max", &ssr_lum_range.y);
}

ADD_TO_DEBUG_MENU(ddgi_debugmenu);

// ---------------------------------------------------------------------------
// Scene GI load (from baked data)
// ---------------------------------------------------------------------------

void DdgiTesting::load_the_gi(IGraphicsTexture* irrad, IGraphicsTexture* depth, std::vector<glm::vec4>& relocate,
                               std::vector<DdgiVolumeGpu>& vols) {
    ASSERT(irrad && depth);
    ASSERT(!vols.empty());

    const int width_probe_space = irrad->get_size().x / ddgiIRRADTILE;
    const int height_probe_space = irrad->get_size().y / ddgiIRRADTILE;
    ASSERT(width_probe_space == depth->get_size().x / ddgiDEPTHTILE);
    ASSERT(height_probe_space == depth->get_size().y / ddgiDEPTHTILE);

    this->myvolumes = std::move(vols);

    DdgiGlobals globals{};
    globals.atlas_x = width_probe_space;
    globals.atlas_y = height_probe_space;
    globals.num_volumes = myvolumes.size();
    globals.relocate_normal_dist = relocate_normal_dist;
    ddgi_globals->upload(&globals, sizeof(globals));
    ddgi_volumes->upload(myvolumes.data(), myvolumes.size() * sizeof(DdgiVolumeGpu));
    theglobals = globals;

    temp_probe_relocate_thing = std::move(relocate);
    ddgi_probe_relocation_offsets->upload(temp_probe_relocate_thing.data(),
                                          sizeof(glm::vec4) * temp_probe_relocate_thing.size());

    if (this->probe_irradiance) {
        this->probe_irradiance->release();
    }
    probe_irradiance = irrad;
    if (this->probe_depth) {
        this->probe_depth->release();
    }
    probe_depth = depth;

    compute_avg_probe_value();
}
