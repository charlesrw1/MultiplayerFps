#ifdef EDITOR_BUILD
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "Framework/Files.h"
#include "AssetTools/AssetDiagnostics.h"
#include "AssetTools/AssetTemplates.h"

static bool write_game_file(const std::string& path, const std::string& content) {
	auto f = FileSys::open_write_game(path);
	if (!f)
		return false;
	f->write(content.data(), content.size());
	f->close();
	return true;
}

struct ScopedTempFile {
	std::string path;
	explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
	~ScopedTempFile() { FileSys::delete_game_file(path); }
	ScopedTempFile(const ScopedTempFile&) = delete;
	ScopedTempFile& operator=(const ScopedTempFile&) = delete;
};

// ---------------------------------------------------------------------------
// Test 1: create_tis_for_png produces a .tis sidecar
// ---------------------------------------------------------------------------

static TestTask test_create_tis_for_png(TestContext& t) {
	const std::string png_path = "textures/__test_auto_import.png";
	const std::string tis_path = "textures/__test_auto_import.tis";

	// Clean up from prior runs / watcher races
	FileSys::delete_game_file(tis_path);
	FileSys::delete_game_file(png_path);
	ScopedTempFile guard_png(png_path);
	ScopedTempFile guard_tis(tis_path);

	// Write .png but immediately create .tis ourselves before the watcher can
	t.require(write_game_file(png_path, "dummy png content"), "wrote dummy .png");

	auto result = AssetTemplates::create_tis_for_png(png_path);
	t.require(result.has_value(), "create_tis_for_png returned a path");
	t.check(*result == tis_path, "returned path matches expected .tis path");
	t.check(FileSys::does_file_exist(tis_path.c_str(), FileSys::GAME_DIR),
			".tis file exists on disk");

	auto second = AssetTemplates::create_tis_for_png(png_path);
	t.check(!second.has_value(), "second call returns nullopt (already exists)");
	co_return;
}
EDITOR_TEST("editor/auto_import_create_tis", 5.f, test_create_tis_for_png);

// ---------------------------------------------------------------------------
// Test 2: .dds without .tis gets Info severity, not Warning
// ---------------------------------------------------------------------------

static TestTask test_missing_tis_is_info(TestContext& t) {
	const std::string dds_path = "textures/__test_diag_notis.dds";

	FileSys::delete_game_file(dds_path);
	t.require(write_game_file(dds_path, "dummy dds"), "wrote dummy .dds");
	ScopedTempFile guard_dds(dds_path);

	AssetDiagnostics::get().scan_dependencies(dds_path);

	auto sev = AssetDiagnostics::get().get_severity(dds_path);
	t.require(sev.has_value(), "diagnostic exists for orphan .dds");
	t.check(*sev == AssetSeverity::Info, "severity is Info, not Warning");

	auto* diags = AssetDiagnostics::get().get_diags(dds_path);
	t.require(diags != nullptr && !diags->empty(), "diagnostic list non-empty");
	t.check(diags->front().message.find(".tis") != std::string::npos,
			"message mentions .tis");

	AssetDiagnostics::get().clear(dds_path);
	co_return;
}
EDITOR_TEST("editor/missing_tis_info_severity", 5.f, test_missing_tis_is_info);

// ---------------------------------------------------------------------------
// Test 3: Info severity does not propagate transitively
// ---------------------------------------------------------------------------

static TestTask test_info_no_transitive_propagation(TestContext& t) {
	const std::string dds_path = "textures/__test_noprop.dds";
	const std::string mi_path = "mats/__test_noprop.mi";

	FileSys::delete_game_file(dds_path);
	FileSys::delete_game_file(mi_path);
	t.require(write_game_file(dds_path, "dummy dds"), "wrote dummy .dds");
	t.require(write_game_file(mi_path,
		"TYPE MaterialInstance\nPARENT defaultPBR.mm\n"
		"VAR diffuseTexture textures/__test_noprop.dds\n"),
		"wrote .mi referencing orphan .dds");
	ScopedTempFile guard_dds(dds_path);
	ScopedTempFile guard_mi(mi_path);

	AssetDiagnostics::get().scan_dependencies(dds_path);
	auto dds_sev = AssetDiagnostics::get().get_severity(dds_path);
	t.require(dds_sev.has_value() && *dds_sev == AssetSeverity::Info,
			  ".dds has Info severity");

	AssetDiagnostics::get().scan_dependencies(mi_path);
	auto mi_sev = AssetDiagnostics::get().get_severity(mi_path);
	t.check(!mi_sev.has_value(), ".mi has no diagnostic (Info did not propagate)");

	AssetDiagnostics::get().clear(dds_path);
	AssetDiagnostics::get().clear(mi_path);
	co_return;
}
EDITOR_TEST("editor/info_no_transitive_propagation", 5.f, test_info_no_transitive_propagation);

// ---------------------------------------------------------------------------
// Test 4: file watcher auto-creates .tis when .png appears
// ---------------------------------------------------------------------------

static TestTask test_filewatcher_auto_import_png(TestContext& t) {
	const std::string png_path = "textures/__test_watcher_auto.png";
	const std::string tis_path = "textures/__test_watcher_auto.tis";

	FileSys::delete_game_file(tis_path);
	FileSys::delete_game_file(png_path);
	ScopedTempFile guard_png(png_path);
	ScopedTempFile guard_tis(tis_path);

	t.require(!FileSys::does_file_exist(tis_path.c_str(), FileSys::GAME_DIR),
			  ".tis does not exist before test");

	t.require(write_game_file(png_path, "dummy png content"),
			  "wrote .png to trigger watcher");

	// File watcher debounces at ~150ms; poll until .tis appears or timeout
	for (int i = 0; i < 30; ++i) {
		co_await t.wait_ticks(2);
		if (FileSys::does_file_exist(tis_path.c_str(), FileSys::GAME_DIR))
			break;
	}

	t.check(FileSys::does_file_exist(tis_path.c_str(), FileSys::GAME_DIR),
			"file watcher auto-created .tis for new .png");
	co_return;
}
EDITOR_TEST("editor/filewatcher_auto_import_png", 10.f, test_filewatcher_auto_import_png);

#endif
