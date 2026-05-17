// Source/IntegrationTests/Tests/Renderer/test_shader_reflection.cpp
//
// Phase 2 B2: validates IGraphicsShader::reflect() against shaders with
// known binding layouts. Required by SDL_GPUShaderCreateInfo in Phase 3.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Render/IGraphicsDevice.h"

static TestTask test_shader_reflection_basic(TestContext& t) {
    eng->load_level("");
    co_await t.wait_ticks(1);

    // MbTexturedV.txt + MbTexturedF.txt, TEXTURE_2D_VERSION:
    //   VS: binding 12 push consts (Model + ViewProj) -> 1 UBO
    //   FS: binding 0 sampler2D (the_texture) + binding 16 push consts -> 1 sampler, 1 UBO
    IGraphicsShader* shader = gfx().create_shader_vert_frag(
        "MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_VERSION");
    t.require(shader != nullptr, "MbTextured shader compiled");

    auto r = shader->reflect();

    t.check(r.vertex.num_uniform_buffers   == 1, "VS: 1 UBO (push consts at binding 12)");
    t.check(r.vertex.num_samplers          == 0, "VS: 0 samplers");
    t.check(r.vertex.num_storage_buffers   == 0, "VS: 0 SSBOs");
    t.check(r.vertex.num_storage_textures  == 0, "VS: 0 images");

    t.check(r.fragment.num_uniform_buffers == 1, "FS: 1 UBO (push consts at binding 16)");
    t.check(r.fragment.num_samplers        == 1, "FS: 1 sampler (the_texture at binding 0)");
    t.check(r.fragment.num_storage_buffers == 0, "FS: 0 SSBOs");
    t.check(r.fragment.num_storage_textures== 0, "FS: 0 images");

    t.check(r.compute.num_uniform_buffers  == 0, "no compute stage in graphics program");

    shader->release();
}
GAME_TEST("renderer/shader_reflection_basic", 10.f, test_shader_reflection_basic);

// fullscreenquad.txt (VS, no uniforms) + BloomDownsampleF.txt (FS):
//   VS: nothing
//   FS: binding 0 sampler2D (srcTexture) + binding 7 group UBO (BloomParamsUbo)
// Exercises: pure-group-UBO path (no per-draw push consts on this shader),
// stage filtering excludes VS, fullscreen-tri VS has zero of everything.
static TestTask test_shader_reflection_bloom(TestContext& t) {
    eng->load_level("");
    co_await t.wait_ticks(1);

    IGraphicsShader* shader = gfx().create_shader_vert_frag(
        "fullscreenquad.txt", "BloomDownsampleF.txt");
    t.require(shader != nullptr, "Bloom shader compiled");

    auto r = shader->reflect();

    t.check(r.vertex.num_uniform_buffers   == 0, "VS: 0 UBOs (fullscreenquad has no uniforms)");
    t.check(r.vertex.num_samplers          == 0, "VS: 0 samplers");
    t.check(r.vertex.num_storage_buffers   == 0, "VS: 0 SSBOs");
    t.check(r.vertex.num_storage_textures  == 0, "VS: 0 images");

    t.check(r.fragment.num_uniform_buffers == 1, "FS: 1 UBO (BloomParamsUbo at binding 7)");
    t.check(r.fragment.num_samplers        == 1, "FS: 1 sampler (srcTexture at binding 0)");
    t.check(r.fragment.num_storage_buffers == 0, "FS: 0 SSBOs");
    t.check(r.fragment.num_storage_textures== 0, "FS: 0 images");

    shader->release();
}
GAME_TEST("renderer/shader_reflection_bloom", 10.f, test_shader_reflection_bloom);

// CullCompute.txt with MAINVIEW define:
//   CS: 1 sampler (depth_pyramid @0),
//       2 UBOs (CullParamsUbo @7, CullDataBuffer @5),
//       5 SSBOs (draw_cmd_out @2, object_inst_indirect @3, CullObjects @4,
//                model_info @6, visbits @7 — different slot pool from UBO @7),
//       0 storage textures.
// Exercises: compute stage, multi-SSBO counting, UBO/SSBO sharing a slot
// number in different binding pools, sampler-but-no-image case.
static TestTask test_shader_reflection_cull_compute(TestContext& t) {
    eng->load_level("");
    co_await t.wait_ticks(1);

    IGraphicsShader* shader = gfx().create_shader_compute(
        "CullCompute.txt", "MAINVIEW");
    t.require(shader != nullptr, "CullCompute shader compiled");

    auto r = shader->reflect();

    // Graphics stages must be zero for a compute program.
    t.check(r.vertex.num_uniform_buffers   == 0, "no vertex stage in compute program (UBO)");
    t.check(r.vertex.num_samplers          == 0, "no vertex stage in compute program (sampler)");
    t.check(r.fragment.num_uniform_buffers == 0, "no fragment stage in compute program (UBO)");
    t.check(r.fragment.num_samplers        == 0, "no fragment stage in compute program (sampler)");

    t.check(r.compute.num_samplers         == 1, "CS: 1 sampler (depth_pyramid)");
    t.check(r.compute.num_storage_textures == 0, "CS: 0 images");
    t.check(r.compute.num_uniform_buffers  == 2, "CS: 2 UBOs (CullParamsUbo, CullDataBuffer)");
    t.check(r.compute.num_storage_buffers  == 5, "CS: 5 SSBOs (draw_cmd, obj_inst, CullObjects, model_info, visbits)");

    shader->release();
}
GAME_TEST("renderer/shader_reflection_cull_compute", 10.f, test_shader_reflection_cull_compute);

// DepthPyramidC.txt: compute shader exercising the storage_textures path.
//   CS: 1 sampler (in_img @0),
//       1 storage texture (out_img @1, r32f image2D — writable),
//       1 UBO (CullParamsUbo @7),
//       0 SSBOs.
// Exercises: image* uniform counted as storage_texture, not sampler.
static TestTask test_shader_reflection_depth_pyramid(TestContext& t) {
    eng->load_level("");
    co_await t.wait_ticks(1);

    IGraphicsShader* shader = gfx().create_shader_compute("DepthPyramidC.txt");
    t.require(shader != nullptr, "DepthPyramidC shader compiled");

    auto r = shader->reflect();

    t.check(r.compute.num_samplers         == 1, "CS: 1 sampler (in_img)");
    t.check(r.compute.num_storage_textures == 1, "CS: 1 storage texture (out_img image2D)");
    t.check(r.compute.num_uniform_buffers  == 1, "CS: 1 UBO (CullParamsUbo)");
    t.check(r.compute.num_storage_buffers  == 0, "CS: 0 SSBOs");

    shader->release();
}
GAME_TEST("renderer/shader_reflection_depth_pyramid", 10.f, test_shader_reflection_depth_pyramid);
