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
