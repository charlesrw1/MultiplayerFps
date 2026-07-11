#pragma once
#include <Framework/Config.h>
extern ConfigVar enable_vsync;
extern ConfigVar shadow_quality_setting;
extern ConfigVar enable_bloom;
extern ConfigVar enable_volumetric_fog;
extern ConfigVar enable_ssao;
extern ConfigVar use_halfres_reflections;
extern ConfigVar dont_use_mdi;
extern ConfigVar renderer_memory_arena_size;
extern ConfigVar r_taa_enabled;
extern ConfigVar r_taa_samples;
extern ConfigVar r_taa_32f;
extern ConfigVar r_specular_ao_intensity;
extern ConfigVar r_skip_gbuffer;
extern ConfigVar force_render_cubemaps;
extern ConfigVar r_drawterrain;
extern ConfigVar r_force_hide_ui;
extern ConfigVar test_thumbnail_model;
extern ConfigVar r_drawdecals;
extern ConfigVar thumbnail_fov;
extern ConfigVar debug_sun_shadow;
extern ConfigVar r_sun_disk_light;
extern ConfigVar debug_specular_reflection;
extern ConfigVar r_no_indirect;
extern ConfigVar r_no_meshbuilders;
extern ConfigVar r_skinned_mats_bone_buffer_size;
extern ConfigVar r_debug_mode;
extern ConfigVar r_drawfog;
extern ConfigVar ddgi_test;
extern ConfigVar ddgi_rt;
extern ConfigVar r_debug_mode;
extern ConfigVar dont_use_mdi;
extern ConfigVar enable_ssr;
extern ConfigVar dont_attach_velocity;

extern ConfigVar r_no_postprocess;
extern ConfigVar r_devicecycle;
extern ConfigVar r_taa_blend;
extern ConfigVar r_taa_stationary_blend;
extern ConfigVar r_taa_adaptive_blend;
extern ConfigVar r_taa_sharpness;
extern ConfigVar r_taa_flicker_remove;
extern ConfigVar r_taa_reproject;
extern ConfigVar r_taa_dilate_velocity;
extern float taa_doc_mult;
extern float taa_doc_vel_bias;
extern float taa_doc_bias;
extern float taa_doc_pow;

extern ConfigVar enable_gl_debug_output;
extern ConfigVar r_taa_jitter_test;
extern ConfigVar r_normal_shaded_debug;
extern ConfigVar r_better_depth_batching;
extern ConfigVar r_no_batching;
extern ConfigVar r_ignore_depth_shader;
extern ConfigVar r_debug_skip_build_scene_data;


extern void build_standard_cpu(Render_Lists& list, Render_Pass& src, Free_List<ROP_Internal>& proxy_list);
extern void build_cascade_cpu(Render_Lists& shadowlist, Render_Pass& shadowpass, Free_List<ROP_Internal>& proxy_list,
							  uint8_t* visiblity);

const int light_frustum_size_x = 8;
const int light_frustum_size_y = 6;