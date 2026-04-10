// Source/IntegrationTests/Tests/Editor/test_road_network.cpp
//
// Integration tests for RoadNetworkComponent:
//   1. Build a small road graph, verify node/edge counts.
//   2. Rebuild mesh without crashing.
//   3. Serialize (save), reload, and confirm the graph round-trips correctly.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Framework/Files.h"

// ---------------------------------------------------------------------------
// Test 1: basic graph operations and mesh rebuild
// ---------------------------------------------------------------------------
static TestTask test_road_network_basic(TestContext& t) {
    Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
    co_await t.wait_ticks(4);
    t.require(eng->get_level() != nullptr, "level loaded for road network test");

    // Spawn an entity and attach a RoadNetworkComponent
    Entity* ent = eng->get_level()->spawn_entity();
    t.require(ent != nullptr, "entity spawned");

    auto* net = ent->create_component<RoadNetworkComponent>();
    t.require(net != nullptr, "RoadNetworkComponent created");

    // ---- add nodes ---------------------------------------------------------
    int id_a = net->add_node({  0.f, 0.f,  0.f });
    int id_b = net->add_node({ 10.f, 0.f,  0.f });
    int id_c = net->add_node({  5.f, 0.f, 10.f });

    t.check(net->get_nodes().size() == 3, "three nodes added");
    t.check(id_a > 0, "node A has valid id");
    t.check(id_b > 0, "node B has valid id");
    t.check(id_c > 0, "node C has valid id");

    // ---- add edges ---------------------------------------------------------
    int e_ab = net->add_edge(id_a, id_b, 6.f, false);
    int e_bc = net->add_edge(id_b, id_c, 6.f, true);  // curved

    t.check(e_ab >= 0, "edge A-B added");
    t.check(e_bc >= 0, "edge B-C (curved) added");
    t.check(net->get_edges().size() == 2, "two edges added");

    // Duplicate edge must be rejected
    int e_ab_dup = net->add_edge(id_a, id_b, 4.f, false);
    t.check(e_ab_dup == -1, "duplicate edge rejected");
    t.check(net->get_edges().size() == 2, "edge count unchanged after duplicate");

    // ---- rebuild mesh (must not crash) -------------------------------------
    net->rebuild_mesh();

    // ---- move a node and rebuild -------------------------------------------
    net->move_node(id_a, { -5.f, 0.f, 0.f });
    const auto* moved = net->find_node(id_a);
    t.require(moved != nullptr, "node A found after move");
    t.check(moved->position.x == -5.f, "node A moved to x=-5");
    net->rebuild_mesh();

    // ---- remove edge and check ---------------------------------------------
    net->remove_edge(e_ab);
    t.check(net->get_edges().size() == 1, "edge count = 1 after remove_edge");
    net->rebuild_mesh();

    // ---- remove node (should also remove its edges) ------------------------
    net->remove_node(id_b);
    t.check(net->get_nodes().size() == 2,  "two nodes remain after removing B");
    t.check(net->get_edges().size() == 0,  "edges cleared after removing B (was connected to e_bc)");
    net->rebuild_mesh();
}
EDITOR_TEST("editor/road_network_basic", 20.f, test_road_network_basic);

// ---------------------------------------------------------------------------
// Test 2: save → reload round-trip
// ---------------------------------------------------------------------------
static TestTask test_road_network_roundtrip(TestContext& t) {
    const char* SAVE_PATH = "_tmp_road_network_test.tmap";
    FileSys::delete_game_file(SAVE_PATH);

    Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
    co_await t.wait_ticks(4);
    t.require(eng->get_level() != nullptr, "level loaded for road network round-trip");

    // Build a simple T-intersection
    Entity* ent = eng->get_level()->spawn_entity();
    auto* net = ent->create_component<RoadNetworkComponent>();
    t.require(net != nullptr, "RoadNetworkComponent created for save test");

    int na = net->add_node({  0.f, 0.f,   0.f });
    int nb = net->add_node({ 20.f, 0.f,   0.f });
    int nc = net->add_node({ 10.f, 0.f,  10.f });
    int nd = net->add_node({ 10.f, 0.f, -10.f });

    net->add_edge(na, nb, 8.f, false);
    net->add_edge(nc, nb, 6.f, false);
    net->add_edge(nd, nb, 6.f, true);  // curved spoke
    net->rebuild_mesh();

    const int saved_nodes = (int)net->get_nodes().size();
    const int saved_edges = (int)net->get_edges().size();

    t.editor().save_level(SAVE_PATH);
    co_await t.wait_ticks(1);

    // Reload the level
    std::string reload_cmd = std::string("open-editor ") + SAVE_PATH;
    Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, reload_cmd.c_str());
    co_await t.wait_ticks(4);
    t.require(eng->get_level() != nullptr, "road network level reloaded");

    // Find the reloaded component
    auto* reloaded_comp = eng->get_level()->find_first_component(&RoadNetworkComponent::StaticType);
    t.require(reloaded_comp != nullptr, "RoadNetworkComponent found after reload");

    auto* rnet = static_cast<RoadNetworkComponent*>(reloaded_comp);
    t.check((int)rnet->get_nodes().size() == saved_nodes,
            ("node count: expected " + std::to_string(saved_nodes) +
             " got " + std::to_string(rnet->get_nodes().size())).c_str());
    t.check((int)rnet->get_edges().size() == saved_edges,
            ("edge count: expected " + std::to_string(saved_edges) +
             " got " + std::to_string(rnet->get_edges().size())).c_str());

    // Verify a curved edge survived correctly
    bool found_curved = false;
    for (const auto& e : rnet->get_edges())
        if (e.curved) found_curved = true;
    t.check(found_curved, "at least one curved edge preserved after reload");

    // Rebuild after reload must not crash
    rnet->rebuild_mesh();
}
EDITOR_TEST("editor/road_network_roundtrip", 25.f, test_road_network_roundtrip);
