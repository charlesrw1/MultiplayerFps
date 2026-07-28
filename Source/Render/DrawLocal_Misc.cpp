#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h" // GL_* constants still referenced; gl* calls go through IGraphicsDevice
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL3/SDL.h>

#include "UI/GUISystemPublic.h" // for GuiSystemPublic::paint
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h" // FIXME
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Render/TaaManager.h"

const GLenum MODEL_INDEX_TYPE_GL = GL_UNSIGNED_SHORT;

//#pragma optimize("", off)

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;

Renderer draw;
RendererPublic* idraw = &draw;

// HOLY CONFIG VARS
ConfigVar enable_vsync("r.enable_vsync", "1", CVAR_BOOL, "enable/disable vsync");
ConfigVar shadow_quality_setting("r.shadow_setting", "0", CVAR_INTEGER, "csm shadow quality", 0, 3);
ConfigVar enable_bloom("r.bloom", "1", CVAR_BOOL, "enable/disable bloom");
ConfigVar enable_volumetric_fog("r.vol_fog", "0", CVAR_BOOL, "enable/disable volumetric fog");
ConfigVar enable_ssao("r.ssao", "1", CVAR_BOOL, "enable/disable screen space ambient occlusion");
ConfigVar use_halfres_reflections("r.halfres_reflections", "1", CVAR_BOOL, "");
ConfigVar dont_use_mdi("r.dont_use_mdi", "0", CVAR_BOOL | CVAR_DEV,
					   "disable multidrawindirect and use drawelements instead");
// 12mb arena
ConfigVar renderer_memory_arena_size("r.mem_arena_size", "12000000", CVAR_INTEGER | CVAR_UNBOUNDED,
									 "size of the renderers memory arena in bytes");

ConfigVar r_taa_enabled("r.taa", "1", CVAR_BOOL, "enable temporal anti aliasing");

static const int MAX_TAA_SAMPLES = 16;
ConfigVar r_taa_samples("r.taa_samples", "4", CVAR_INTEGER, "", 2, MAX_TAA_SAMPLES);
ConfigVar r_taa_32f("r.taa_32f", "0", CVAR_BOOL, "use 32 bit scene motion buffer instead of 16 bit");

// basically:
// diffuse_ao = pow(ao, ssao.intensity)
// specular_ao = pow(diffuse_ao,r_specular_ao_intensity)

ConfigVar r_specular_ao_intensity("r.specular_ao_intensity", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar r_debug_skip_build_scene_data("r.debug_skip_build_scene_data", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_gbuffer("r_skip_gbuffer", "0", CVAR_BOOL, "");

ConfigVar force_render_cubemaps("r.force_cubemap_render", "0", CVAR_BOOL | CVAR_DEV,
								"force cubemaps to re-render, treated like a flag and not a setting");

ConfigVar r_drawterrain("r.drawterrain", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of terrain");
ConfigVar r_force_hide_ui("r.force_hide_ui", "0", CVAR_BOOL, "disable ui drawing");
ConfigVar test_thumbnail_model("test_thumbnail_model", "", CVAR_DEV, "");

ConfigVar r_drawdecals("r.drawdecals", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of decals");

ConfigVar thumbnail_fov("thumbnail_fov", "30", CVAR_FLOAT | CVAR_UNBOUNDED, "");

ConfigVar debug_sun_shadow("r.debug_csm", "0", CVAR_BOOL | CVAR_DEV, "debug csm shadow rendering");
ConfigVar r_sun_disk_light("r.sun_disk_light", "1", CVAR_BOOL | CVAR_DEV, "use disk area light for sun specular");
ConfigVar debug_specular_reflection("r.debug_specular", "0", CVAR_BOOL | CVAR_DEV, "debug specular lighting");
ConfigVar r_no_indirect("r.no_indirect", "0", CVAR_BOOL, "");

ConfigVar r_no_meshbuilders("r_no_meshbuilders", "0", CVAR_BOOL | CVAR_DEV, "");
// 128 bones * 100 characters * 2 (double bffer) =
ConfigVar r_skinned_mats_bone_buffer_size("r.skinned_mats_bone_buffer_size", "25600",
										  CVAR_INTEGER | CVAR_UNBOUNDED | CVAR_READONLY, "");

ConfigVar r_better_depth_batching("r.better_depth_batching", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_no_batching("r.no_batching", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_ignore_depth_shader("r_ignore_depth_shader", "0", CVAR_BOOL | CVAR_DEV, "...");

ConfigVar enable_gl_debug_output("enable_gl_debug_output", "1", CVAR_BOOL, "");
ConfigVar r_taa_jitter_test("r.taa_jitter_test", "0", CVAR_INTEGER, "", 0, 4);
ConfigVar r_normal_shaded_debug("r.normal_shaded_debug", "1", CVAR_BOOL, "");
ConfigVar log_shader_compiles("log_shader_compiles", "0", CVAR_BOOL, "");

ConfigVar r_debug_mode("r.debug_mode", "0", CVAR_INTEGER | CVAR_DEV,
					   "render debug mode, see Draw.cpp for DEBUG_x values, 0 to disable", 0, 200);

ConfigVar r_drawfog("r.drawfog", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of fog");

ConfigVar ddgi_test("dt", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar ddgi_rt("ddrt", "0", CVAR_BOOL | CVAR_DEV, "");

extern void build_frustum_for_cascade(Frustum& f, int index);

RenderWindowBackend* RenderWindowBackend::inst = nullptr;
#include "RenderWindowLocal.h"

TaaManager r_taa_manager;

// BuildSceneData_CpuFast batch/draw implementation -> DrawLocal_BatchScene.cpp
// BuildSceneData_CpuFast culling/shadow/removal implementation -> DrawLocal_CullShadow.cpp
BuildSceneData_CpuFast* BuildSceneData_CpuFast::inst = nullptr;
