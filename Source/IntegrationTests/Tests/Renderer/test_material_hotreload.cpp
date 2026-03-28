// Source/IntegrationTests/Tests/Renderer/test_material_hotreload.cpp
//
// Integration tests for material hot reloading.
// Tests 1-2: manual reload via g_assets.reload_sync  (GAME_TEST)
// Test 3:    OS file-watcher driven reload            (EDITOR_TEST, editor build only)
//
// First-run: launch with --promote to capture screenshot baselines.
// Subsequent runs: diffs against baselines; if reload is broken the _after
// screenshot will diverge from its golden.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/CameraComponent.h"
#include "Render/MaterialPublic.h"
#include "Assets/AssetDatabase.h"
#include "Framework/Files.h"
#include "Render/Model.h"
#include "Render/RenderConfigVars.h"
// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Write |content| to a game-relative path.  Returns false on failure.
static bool write_game_file(const std::string& path, const std::string& content) {
	auto f = FileSys::open_write_game(path);
	if (!f)
		return false;
	f->write(content.data(), content.size());
	f->close();
	return true;
}

// RAII: deletes the temp file when the coroutine frame is destroyed.
struct ScopedTempFile
{
	std::string path;
	explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
	~ScopedTempFile() { FileSys::delete_game_file(path); }
	ScopedTempFile(const ScopedTempFile&) = delete;
	ScopedTempFile& operator=(const ScopedTempFile&) = delete;
};

// Spawns a cube at the origin and a camera 4 units back looking at it.
// Call after co_await t.load_level(...).
// Returns the MeshComponent so the caller can set material overrides.
static MeshComponent* setup_test_scene() {
	auto* cube_ent = eng->get_level()->spawn_entity();
	auto* mesh = cube_ent->create_component<MeshComponent>();
	mesh->set_model_str("eng/cube.cmdl");

	auto skybox = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
	skybox->set_is_skybox(true);
	skybox->get_owner()->set_ls_scale(glm::vec3(2000));
	skybox->set_model(Model::load("eng/skydome.cmdl"));

	extern ConfigVar r_debug_mode;
	r_taa_enabled.set_bool(false);
	r_debug_mode.set_integer(5 /* albedo */);

	auto* cam_ent = eng->get_level()->spawn_entity();
	auto* cam = cam_ent->create_component<CameraComponent>();
	cam->set_is_enabled(true);
	cam_ent->set_ws_position({0.f, 1.f, 4.f});

	return mesh;
}

// ---------------------------------------------------------------------------
// Test 1: MaterialInstance (.mi) hot reload via reload_sync
// ---------------------------------------------------------------------------
// Writes a red .mi, screenshots it, rewrites it blue, calls reload_sync,
// screenshots again.  Baselines: red cube then blue cube.

static TestTask test_hotreload_material_instance(TestContext& t) {
	eng->load_level("");
	ASSERT(eng->get_level()->get_source_asset_name() == "");
	MeshComponent* mesh = setup_test_scene();

	const std::string inst_path = "mats/test_hotreload_inst.mi";
	ScopedTempFile guard(inst_path);

	const std::string red_mi =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 255 0 0 255\n";
	t.require(write_game_file(inst_path, red_mi), "wrote red .mi");

	auto mat = g_assets.find_sync_sptr<MaterialInstance>(inst_path);
	t.require(mat != nullptr, ".mi asset loaded");
	mesh->set_material_override(mat.get());

	co_await t.wait_ticks(1);
	co_await t.capture_screenshot("hotreload_inst_before"); // golden: red cube

	const std::string blue_mi =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 0 0 255 255\n";
	t.require(write_game_file(inst_path, blue_mi), "overwrote with blue .mi");

	g_assets.reload_sync(mat.get());

	co_await t.wait_ticks(1);
	co_await t.capture_screenshot("hotreload_inst_after"); // golden: blue cube
}
GAME_TEST("renderer/hotreload_material_instance", 20.f, test_hotreload_material_instance);

// ---------------------------------------------------------------------------
// Test 2: MasterMaterial (.mm) hot reload via reload_sync
// ---------------------------------------------------------------------------
// Writes a minimal .mm with solid-red GLSL, screenshots it, rewrites with
// solid-green GLSL, calls reload_sync (triggers shader recompile and cascades
// to all dependent instances), screenshots again.

static TestTask test_hotreload_master_material(TestContext& t) {
	eng->load_level("");

	MeshComponent* mesh = setup_test_scene();

	const std::string master_path = "test_hotreload_master.mm";
	ScopedTempFile guard(master_path);

	const std::string red_mm =
		"TYPE MaterialMaster\n"
		"_FS_BEGIN\n"
		"void FSmain()\n"
		"{\n"
		"    BASE_COLOR = vec3(1.0, 0.0, 0.0);\n"
		"    NORMALMAP  = vec3(0.5, 0.5, 1.0);\n"
		"    ROUGHNESS  = 0.8;\n"
		"    METALLIC   = 0.0;\n"
		"    AOMAP      = 1.0;\n"
		"}\n"
		"_FS_END\n";
	t.require(write_game_file(master_path, red_mm), "wrote red .mm");

	auto mat = g_assets.find_sync_sptr<MaterialInstance>(master_path);
	t.require(mat != nullptr, ".mm asset loaded");
	mesh->set_material_override(mat.get());

	co_await t.wait_ticks(1);
	co_await t.capture_screenshot("hotreload_master_before"); // golden: solid red

	const std::string green_mm =
		"TYPE MaterialMaster\n"
		"_FS_BEGIN\n"
		"void FSmain()\n"
		"{\n"
		"    BASE_COLOR = vec3(0.0, 1.0, 0.0);\n"
		"    NORMALMAP  = vec3(0.5, 0.5, 1.0);\n"
		"    ROUGHNESS  = 0.8;\n"
		"    METALLIC   = 0.0;\n"
		"    AOMAP      = 1.0;\n"
		"}\n"
		"_FS_END\n";
	t.require(write_game_file(master_path, green_mm), "overwrote with green .mm");

	// reload_sync re-parses the .mm, recompiles the shader, and auto-reloads
	// all MaterialInstances that point to this master.
	g_assets.reload_sync(mat.get());

	co_await t.wait_ticks(1); // extra ticks for shader compile latency
	co_await t.capture_screenshot("hotreload_master_after"); // golden: solid green
}
GAME_TEST("renderer/hotreload_master_material", 30.f, test_hotreload_master_material);

// ---------------------------------------------------------------------------
// Test 3: OS file-watcher driven hot reload (editor build only)
// ---------------------------------------------------------------------------
// Writes a red .mi, writes green over it, then waits for AssetRegistrySystem's
// Win32 FindFirstChangeNotification watcher to detect the change and call
// reload_sync automatically.  No manual reload call in the test.
//
// Note: the watcher has a 5-second rate-limit between cycles; the 45-second
// timeout covers worst-case latency.

#ifdef EDITOR_BUILD
static TestTask test_hotreload_os_filewatcher(TestContext& t) {
	eng->load_level("");

	MeshComponent* mesh = setup_test_scene();

	const std::string inst_path = "mats/test_hotreload_os.mi";
	ScopedTempFile guard(inst_path);

	const std::string red_mi =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 255 0 0 255\n";
	t.require(write_game_file(inst_path, red_mi), "wrote red .mi for OS test");

	auto mat = g_assets.find_sync_sptr<MaterialInstance>(inst_path);
	t.require(mat != nullptr, "OS test .mi loaded");
	mesh->set_material_override(mat.get());

	co_await t.wait_ticks(1);
	co_await t.capture_screenshot("hotreload_os_before"); // golden: red cube

	// Overwrite on disk — the OS watcher will pick this up automatically.
	const std::string green_mi =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 0 255 0 255\n";
	t.require(write_game_file(inst_path, green_mi), "overwrote with green .mi");

	// Bridge MulticastDelegate<MaterialInstance*> -> MulticastDelegate<> so we
	// can use ctx.wait_for(), which only accepts the no-arg delegate type.
	// Filter to our specific material to avoid false positives from other reloads.
	MulticastDelegate<> reload_signal;
	MaterialInstance* expected = mat.get();
	MaterialInstance::on_material_loaded.add(
		&reload_signal, [&reload_signal, expected](MaterialInstance* m) {
			if (m == expected)
				reload_signal.invoke();
		});

	co_await t.wait_for(reload_signal); // suspends until watcher fires

	// Remove our on_material_loaded listener (DelegateAwaitable auto-removed
	// its own listener from reload_signal, but not from on_material_loaded).
	MaterialInstance::on_material_loaded.remove(&reload_signal);

	co_await t.wait_ticks(1); // let GPU material buffer catch up
	co_await t.capture_screenshot("hotreload_os_after"); // golden: green cube
}
GAME_TEST("renderer/hotreload_os_filewatcher", 5.f, test_hotreload_os_filewatcher);
#endif
