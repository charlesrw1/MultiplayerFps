// Integration tests for the runtime static-prop strip pass (Level::insert_unserialized_entities_into_level_internal).
//
// Fixture: Data/test/static_strip_fixture.tmap
//   3 anonymous bare MeshComponent entities (cube.cmdl, no collision) — strippable
//   1 named MeshComponent ("keep_me")                                  — NOT strippable (editor_name set)
//   1 SkylightComponent                                                — NOT strippable (not a MeshComponent)
//
// Each .tmap obj entry produces an Entity + 1 Component pair (see SerializeNew::unserialize_from_json),
// so the fixture loads 5 Entity + 5 Component = 10 BaseUpdater objects.
//
// strip/runtime    — runtime mode strips the 3 anonymous props (6 BaseUpdater removed); pool gets 3 entries.
// strip/cvar_off   — r_disable_static_strip disables the pass; same map loads with all 10 BaseUpdater present.
// strip/editor_off — editor mode never strips; same map loads with all 10 BaseUpdater present.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "DebugConsole.h"

#include "Level.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Framework/Config.h"

namespace {
constexpr const char* kFixture = "test/static_strip_fixture.tmap";
constexpr int kTotalLoadedObjs = 10; // 5 Entity + 5 Component
constexpr int kStrippedObjs    = 6;  // 3 Entity + 3 Component
constexpr int kKeptObjs        = kTotalLoadedObjs - kStrippedObjs; // 4

int count_named_entities(const char* name) {
	int n = 0;
	for (auto* o : eng->get_level()->get_all_objects()) {
		if (auto* e = o->cast_to<Entity>())
			if (e->get_editor_name() == name)
				n++;
	}
	return n;
}
} // namespace

// Runtime mode: the 3 anonymous props are stripped into the pool; the named entity
// + its MeshComponent and the SkylightComponent's Entity + SkylightComponent stay.
static TestTask test_static_strip_runtime(TestContext& t) {
	t.require(eng->load_level(kFixture), "fixture map loaded");
	co_await t.wait_ticks(2);
	t.require(eng->get_level() != nullptr, "level present");

	const int kept = (int)eng->get_level()->get_all_objects().num_used;
	t.check(kept == kKeptObjs, "only non-strippable entities remain in all_world_ents");
	t.check(eng->get_level()->get_static_pool().size() == 3, "static pool received the 3 anonymous props");
	t.check(count_named_entities("keep_me") == 1, "named entity preserved");
}
GAME_TEST("strip/runtime", 10.f, test_static_strip_runtime);

// Cvar gate: setting r_disable_static_strip restores the pre-strip behaviour at runtime
// (every entity stays in all_world_ents, pool is empty).
static TestTask test_static_strip_cvar_off(TestContext& t) {
	auto* cvar = VarMan::get()->find("r_disable_static_strip");
	t.require(cvar != nullptr, "r_disable_static_strip cvar registered");
	const bool saved = cvar->get_bool();
	cvar->set_bool(true);

	t.require(eng->load_level(kFixture), "fixture map loaded with strip disabled");
	co_await t.wait_ticks(2);

	const int kept = (int)eng->get_level()->get_all_objects().num_used;
	t.check(kept == kTotalLoadedObjs, "all entities preserved when strip disabled");
	t.check(eng->get_level()->get_static_pool().size() == 0, "pool empty when strip disabled");

	cvar->set_bool(saved);
}
GAME_TEST("strip/cvar_off", 10.f, test_static_strip_cvar_off);

// Editor mode never strips — selection / naming / gizmos require every Entity to stay live.
static TestTask test_static_strip_editor_off(TestContext& t) {
	std::string cmd = "open-editor ";
	cmd += kFixture;
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, cmd.c_str());
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "editor level present");

	const int kept = (int)eng->get_level()->get_all_objects().num_used;
	t.check(kept >= kTotalLoadedObjs, "editor preserves every loaded entity");
	t.check(eng->get_level()->get_static_pool().size() == 0, "pool empty in editor mode");
}
EDITOR_TEST("strip/editor_off", 15.f, test_static_strip_editor_off);
