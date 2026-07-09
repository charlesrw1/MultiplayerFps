// Source/IntegrationTests/Tests/Editor/test_bake_gi.cpp
//
// Editor test for the `bake_probes` (cubemap) and `bake_ddgi` (DDGI atlas)
// console commands. Loads maps/bake_probes_test.tmap, snaps the editor
// camera onto the level's "CAMERA" SpawnerComponent, bakes both, then diffs
// the scene viewport against a golden. First run requires --promote.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/LevelEditorCamera.h"
#include "Render/RenderConfigVars.h"
#include "Framework/Config.h"
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

extern ConfigVar r_ddgi_halfres;     // "r.ddgi_halfres"
extern ConfigVar ddgi_test;          // "dt"
extern ConfigVar enable_ssr;         // "r.ssr"
extern ConfigVar ddgi_probe_debug;   // "ddgi_probe_debug"
extern ConfigVar g_window_w;         // "vid.width"
extern ConfigVar g_window_h;         // "vid.height"
extern ConfigVar ed_force_viewport_w; // "ed.force_viewport_w"
extern ConfigVar ed_force_viewport_h; // "ed.force_viewport_h"
// r_taa_enabled comes from RenderConfigVars.h ("r.taa").

static Entity* find_entity_by_editor_name(const char* name) {
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* e = obj->cast_to<Entity>())
			if (e->get_editor_name() == name)
				return e;
	}
	return nullptr;
}

// RAII guard so cvar overrides are reverted on every exit path — including
// when t.require() aborts the coroutine via TestAbortException.
struct CvarRestore {
	struct IntEntry { ConfigVar* var; int prev; };
	std::vector<IntEntry> ints;
	void save_int(ConfigVar& v) { ints.push_back({&v, v.get_integer()}); }
	void save_bool(ConfigVar& v) { ints.push_back({&v, v.get_integer()}); }
	~CvarRestore() {
		for (auto& e : ints) e.var->set_integer(e.prev);
	}
};

static TestTask test_bake_probes_and_ddgi(TestContext& t) {
	CvarRestore restore;
	restore.save_bool(ddgi_probe_debug);
	restore.save_bool(ddgi_test);
	restore.save_bool(enable_ssr);
	restore.save_bool(r_taa_enabled);
	restore.save_bool(r_ddgi_halfres);
	restore.save_int(g_window_w);
	restore.save_int(g_window_h);
	restore.save_int(ed_force_viewport_w);
	restore.save_int(ed_force_viewport_h);

	// Deterministic render config — matches the user's intended bake setup.
	ddgi_probe_debug.set_bool(true);
	ddgi_test.set_bool(true);
	enable_ssr.set_bool(false);
	r_taa_enabled.set_bool(false);
	r_ddgi_halfres.set_bool(false);

	// Pin the scene viewport to a fixed resolution so the golden is independent of
	// the imgui dock layout (editor.ini). Without this the scene panel size is
	// whatever the saved dock layout produces — a layout change silently resizes
	// the render target, breaking the golden compare (and, at odd widths, tripped
	// a heap overflow in the rgb8 readback). The window stays large enough that the
	// panel exists; the override drives the actual render/screenshot size.
	g_window_w.set_integer(1280);
	g_window_h.set_integer(480);
	ed_force_viewport_w.set_integer(600);
	ed_force_viewport_h.set_integer(400);
	co_await t.wait_ticks(2);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor maps/bake_probes_test.tmap");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "level loaded");

	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	t.require(doc != nullptr, "editor doc is null");

	Entity* cam = find_entity_by_editor_name("CAMERA");
	t.require(cam != nullptr, "CAMERA entity not found in bake_probes_test.tmap");

	// Editor camera matches CameraComponent::get_view convention: view = inverse(ws_transform),
	// so the entity looks down its local -Z axis with +Y up.
	const glm::mat4 xform = cam->get_ws_transform();
	const glm::vec3 pos = glm::vec3(xform[3]);
	const glm::vec3 fwd = glm::normalize(-glm::vec3(xform[2]));

	CameraSnapshot snap;
	snap.is_ortho = false;
	snap.position = pos;
	snap.front = fwd;
	snap.up = glm::vec3(0.f, 1.f, 0.f);
	// User_Camera regenerates `front` each tick from yaw/pitch via AnglesToVector,
	// so we must precompute the matching angles or the heading snaps back next frame.
	snap.yaw = std::atan2(fwd.z, fwd.x);
	snap.pitch = std::asin(glm::clamp(fwd.y, -1.f, 1.f));
	snap.orbit_mode = false;
	doc->ed_cam.apply_snapshot(snap, /*teleport=*/true);


	// bake_ddgi runs first so the irradiance volumes are populated before the
	// cubemaps capture the scene — otherwise the baked cubemap reflections show
	// the unlit (pre-DDGI) scene.
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "bake_ddgi");
	co_await t.wait_ticks(2);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "bake_probes");
	co_await t.wait_ticks(2);

	// DDGI bake traces probes with randomized rays — a tiny handful of pixels
	// drift between runs (max_delta ~66 on ~3 pixels). Opt into the warn band
	// so that drift is reported but doesn't fail the test.
	co_await t.capture_screenshot("bake_probes_test", 80, 0.001f);
}
EDITOR_TEST("editor/bake_probes_test", 60.f, test_bake_probes_and_ddgi);
