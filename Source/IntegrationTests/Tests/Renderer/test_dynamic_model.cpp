// Source/IntegrationTests/Tests/Renderer/test_dynamic_model.cpp
//
// Integration tests for the dynamic (procedural) model API.
//
// Tests cover:
//   1. ModelBuilder geometry construction
//   2. ModelMan::create_dynamic_model — model is live and renderable
//   3. Live-count tracking (get_num_dynamic_models)
//   4. Explicit free via free_dynamic_model and RAII via DynamicModelUniquePtr
//   5. Visual smoke-test: dynamic model appears in a screenshot

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/CameraComponent.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Render/DynamicModelPtr.h"
#include "Framework/Config.h"

// ---------------------------------------------------------------------------
// Test 1 (editor): lifecycle — create, count, RAII free
// ---------------------------------------------------------------------------
// Opens a blank scene, creates two dynamic models (triangle + quad), verifies
// live-count bookkeeping, then drops the DynamicModelUniquePtrs and confirms
// the count returns to its initial value.

static TestTask test_dynamic_model_lifecycle(TestContext& t) {
    // Open a blank level in editor mode.
    Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
    co_await t.wait_ticks(4);
    t.require(eng->get_level() != nullptr, "level loaded for dynamic model test");

    const int initial_count = g_modelMgr.get_num_dynamic_models();

    // ---- Build a triangle -----------------------------------------------
    ModelBuilder tri_builder;
    uint16_t t0 = tri_builder.add_vertex({-0.5f, -0.5f, 0.f}, {0.f, 1.f}, {0.f, 0.f, 1.f});
    uint16_t t1 = tri_builder.add_vertex({ 0.5f, -0.5f, 0.f}, {1.f, 1.f}, {0.f, 0.f, 1.f});
    uint16_t t2 = tri_builder.add_vertex({ 0.f,   0.5f, 0.f}, {0.5f, 0.f}, {0.f, 0.f, 1.f});
    tri_builder.add_triangle(t0, t1, t2);

    t.check(tri_builder.get_vertex_count() == 3, "triangle builder: 3 vertices");
    t.check(tri_builder.get_index_count()  == 3, "triangle builder: 3 indices");

    // ---- Create first dynamic model (triangle) ---------------------------
    DynamicModelUniquePtr dyn_tri(
        g_modelMgr.create_dynamic_model(tri_builder, "test_triangle"));
    t.require(dyn_tri != nullptr, "triangle dynamic model created");
    t.check(g_modelMgr.get_num_dynamic_models() == initial_count + 1,
            "live count incremented after first create");

    // ---- Build a quad and create a second dynamic model -----------------
    ModelBuilder quad_builder;
    uint16_t q0 = quad_builder.add_vertex({-0.5f,  0.5f, 0.f}, {0.f, 0.f}, {0.f, 0.f, 1.f});
    uint16_t q1 = quad_builder.add_vertex({ 0.5f,  0.5f, 0.f}, {1.f, 0.f}, {0.f, 0.f, 1.f});
    uint16_t q2 = quad_builder.add_vertex({ 0.5f, -0.5f, 0.f}, {1.f, 1.f}, {0.f, 0.f, 1.f});
    uint16_t q3 = quad_builder.add_vertex({-0.5f, -0.5f, 0.f}, {0.f, 1.f}, {0.f, 0.f, 1.f});
    quad_builder.add_quad(q0, q1, q2, q3);

    t.check(quad_builder.get_vertex_count() == 4, "quad builder: 4 vertices");
    t.check(quad_builder.get_index_count()  == 6, "quad builder: 6 indices (2 tris)");

    DynamicModelUniquePtr dyn_quad(
        g_modelMgr.create_dynamic_model(quad_builder, "test_quad"));
    t.require(dyn_quad != nullptr, "quad dynamic model created");
    t.check(g_modelMgr.get_num_dynamic_models() == initial_count + 2,
            "live count is initial+2 after second create");

    // ---- Verify model has geometry / is valid ---------------------------
    t.check(dyn_tri->get_num_parts() == 1,  "triangle model: 1 submesh");
    t.check(dyn_quad->get_num_parts() == 1, "quad model: 1 submesh");
    t.check(dyn_tri->get_num_lods()  == 1,  "triangle model: 1 LOD");
    t.check(dyn_quad->get_num_lods() == 1,  "quad model: 1 LOD");
    t.check(dyn_tri->is_loaded_in_memory(),  "triangle model reports loaded");
    t.check(dyn_quad->is_loaded_in_memory(), "quad model reports loaded");

    // ---- Free first model via explicit reset (RAII) ----------------------
    dyn_tri.reset();
    t.check(g_modelMgr.get_num_dynamic_models() == initial_count + 1,
            "live count decremented after first free");

    // ---- Free second model via explicit reset ----------------------------
    dyn_quad.reset();
    t.check(g_modelMgr.get_num_dynamic_models() == initial_count,
            "live count back to initial after both models freed");
}
EDITOR_TEST("editor/dynamic_model_lifecycle", 20.f, test_dynamic_model_lifecycle);

// ---------------------------------------------------------------------------
// Test 2 (game): render smoke — dynamic quad appears in a screenshot
// ---------------------------------------------------------------------------
// Creates a quad-shaped dynamic model, attaches it to a MeshComponent, and
// captures a screenshot.  The quad uses the engine's fallback (grey) material.

static TestTask test_dynamic_model_render(TestContext& t) {
    eng->load_level("");

    // ---- Camera ----------------------------------------------------------
    auto* cam_ent = eng->get_level()->spawn_entity();
    auto* cam     = cam_ent->create_component<CameraComponent>();
    cam->set_is_enabled(true);
    cam_ent->set_ws_position({0.f, 0.f, 3.f});

    // ---- Build a unit quad facing the camera (Z+) -----------------------
    ModelBuilder builder;
    uint16_t q0 = builder.add_vertex({-0.5f,  0.5f, 0.f}, {0.f, 0.f}, {0.f, 0.f, 1.f});
    uint16_t q1 = builder.add_vertex({ 0.5f,  0.5f, 0.f}, {1.f, 0.f}, {0.f, 0.f, 1.f});
    uint16_t q2 = builder.add_vertex({ 0.5f, -0.5f, 0.f}, {1.f, 1.f}, {0.f, 0.f, 1.f});
    uint16_t q3 = builder.add_vertex({-0.5f, -0.5f, 0.f}, {0.f, 1.f}, {0.f, 0.f, 1.f});
    builder.add_quad(q0, q1, q2, q3);

    const int before = g_modelMgr.get_num_dynamic_models();
    DynamicModelUniquePtr dyn(g_modelMgr.create_dynamic_model(builder, "render_test_quad"));
    t.require(dyn != nullptr, "render test: dynamic model created");
    t.check(g_modelMgr.get_num_dynamic_models() == before + 1, "render test: count incremented");

    // ---- Attach to a mesh component -------------------------------------
    auto* ent  = eng->get_level()->spawn_entity();
    auto* mesh = ent->create_component<MeshComponent>();
    mesh->set_model(dyn.get());

    co_await t.wait_ticks(2);
    co_await t.capture_screenshot("dynamic_model_render_quad");

    // ---- Detach model before freeing to avoid dangling render reference --
    mesh->set_model(nullptr);
    co_await t.wait_ticks(1);

    dyn.reset();
    t.check(g_modelMgr.get_num_dynamic_models() == before, "render test: count restored after free");
}
GAME_TEST("renderer/dynamic_model_render", 15.f, test_dynamic_model_render);
