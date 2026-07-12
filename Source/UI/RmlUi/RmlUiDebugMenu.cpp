// Debug menu that flips through a fixed set of self-contained RmlUi example
// documents (Data/ui/examples/*.rml) - lets you eyeball layout/text/controls
// without a level or Lua glue. See docs/ui/rmlui_agent_guide.md.
#include "RmlUiSystem.h"
#include "Framework/Config.h"
#include "Render/RmlUiRenderInterface.h"
#include "Scripting/ScriptManager.h"
#include "imgui.h"

namespace {
struct RmlUiExample {
	const char* display_name;
	const char* doc_path; // relative to Data/, passed to RmlUiSystem::load_document
};

// Static docs only - no data-model, so RmlUiSystem::load_document (plain
// C++, no Lua involved) is enough to show them.
const RmlUiExample rmlui_examples[] = {
	{ "Typography", "ui/examples/ex_typography.rml" },
	{ "Flexbox layout", "ui/examples/ex_flexbox.rml" },
	{ "Controls", "ui/examples/ex_controls.rml" },
	{ "Compass HUD", "ui/examples/ex_compass.rml" },
	{ "Source-style Settings", "ui/examples/ex_source_settings.rml" },
};
constexpr int rmlui_example_count = sizeof(rmlui_examples) / sizeof(rmlui_examples[0]);

// Docs that bind a Lua data-model (data-model="..." + {{ }} in the .rml) -
// RmlUiSystem::load_document has no data-model support, so these must be
// opened via rmlui.contexts["main"]:OpenDataModel from Lua instead. Indices
// continue on from the static examples above: rmlui_example_count +
// (position in this array). tick_fn (optional) is called once per debug-menu
// ImGui frame while the entry is the active selection - the RmlUi Lua
// binding has no timer API of its own, so a Lua-side script that wants a
// steady per-second update (see hud_corners_demo.lua) accumulates real dt
// itself across these calls rather than relying on a CSS-transition
// metronome, which turned out to have a silent failure mode (see that
// file's header comment).
struct RmlUiLuaExample {
	const char* display_name;
	const char* open_fn;
	const char* close_fn;
	const char* tick_fn; // nullptr if the doc doesn't need per-frame updates
};
const RmlUiLuaExample rmlui_lua_examples[] = {
	{ "Lua data model demo", "rmlui_demo_open()", "rmlui_demo_close()", nullptr },
	{ "HUD Corners", "hud_corners_open()", "hud_corners_close()", "hud_corners_tick()" },
};
constexpr int rmlui_lua_example_count = sizeof(rmlui_lua_examples) / sizeof(rmlui_lua_examples[0]);
constexpr int rmlui_lua_example_base = rmlui_example_count;
constexpr int rmlui_total_example_count = rmlui_example_count + rmlui_lua_example_count;

int rmlui_example_index = -1; // -1 = nothing shown yet
RmlDocHandle rmlui_example_doc = RML_INVALID_DOC;

// ScriptManager::reload_from_content runs a snippet directly against the
// engine's live lua_State, so the debug menu can drive a Lua-backed
// example's open()/close()/tick() globals the same way the "rmlui_demo"
// console command does.
void rmlui_run_lua(const char* snippet) {
	if (ScriptManager::inst)
		ScriptManager::inst->reload_from_content(snippet, "rmlui_debug_menu");
}

void rmlui_show_example(int index) {
	if (!RmlUiSystem::inst)
		return;
	if (rmlui_example_doc != RML_INVALID_DOC) {
		RmlUiSystem::inst->close_document(rmlui_example_doc);
		rmlui_example_doc = RML_INVALID_DOC;
	}
	if (rmlui_example_index >= rmlui_lua_example_base)
		rmlui_run_lua(rmlui_lua_examples[rmlui_example_index - rmlui_lua_example_base].close_fn);
	rmlui_example_index = index;
	if (index >= rmlui_lua_example_base && index < rmlui_total_example_count) {
		rmlui_run_lua(rmlui_lua_examples[index - rmlui_lua_example_base].open_fn);
		return;
	}
	if (index < 0 || index >= rmlui_example_count)
		return;
	rmlui_example_doc = RmlUiSystem::inst->load_document(rmlui_examples[index].doc_path);
	if (rmlui_example_doc != RML_INVALID_DOC)
		RmlUiSystem::inst->show_document(rmlui_example_doc);
}
} // namespace

void rmlui_examples_debug_menu() {
	if (!RmlUiSystem::inst) {
		ImGui::Text("RmlUiSystem not initialized");
		return;
	}

	const char* current_name = "(none)";
	if (rmlui_example_index >= rmlui_lua_example_base)
		current_name = rmlui_lua_examples[rmlui_example_index - rmlui_lua_example_base].display_name;
	else if (rmlui_example_index >= 0)
		current_name = rmlui_examples[rmlui_example_index].display_name;
	ImGui::Text("Current: %s", current_name);

	if (ImGui::Button("<< Prev")) {
		int next = (rmlui_example_index <= 0) ? rmlui_total_example_count - 1 : rmlui_example_index - 1;
		rmlui_show_example(next);
	}
	ImGui::SameLine();
	if (ImGui::Button("Next >>")) {
		int next = (rmlui_example_index < 0 || rmlui_example_index >= rmlui_total_example_count - 1) ? 0 : rmlui_example_index + 1;
		rmlui_show_example(next);
	}
	ImGui::SameLine();
	if (ImGui::Button("Hide")) {
		rmlui_show_example(-1);
	}

	for (int i = 0; i < rmlui_example_count; i++) {
		if (ImGui::Selectable(rmlui_examples[i].display_name, i == rmlui_example_index))
			rmlui_show_example(i);
	}
	for (int i = 0; i < rmlui_lua_example_count; i++) {
		int index = rmlui_lua_example_base + i;
		if (ImGui::Selectable(rmlui_lua_examples[i].display_name, index == rmlui_example_index))
			rmlui_show_example(index);
	}

	// Drive the active example's per-frame update, if it has one - only
	// reachable while this debug section is expanded (the enclosing
	// ImGui::CollapsingHeader in Debug_Interface_Impl::draw gates whether
	// this whole function runs at all), so a doc kept open after collapsing
	// the section will stop ticking until it's expanded again. Acceptable
	// for a debug-only example; a real gameplay HUD would need an
	// always-on driver instead (e.g. an Entity Component with
	// set_ticking(true)).
	if (rmlui_example_index >= rmlui_lua_example_base) {
		const char* tick_fn = rmlui_lua_examples[rmlui_example_index - rmlui_lua_example_base].tick_fn;
		if (tick_fn)
			rmlui_run_lua(tick_fn);
	}
}
ADD_TO_DEBUG_MENU(rmlui_examples_debug_menu);

// Per-frame RenderInterface call counts (RmlUiRenderInterface.cpp) - use
// this to see whether an animation/interaction is forcing geometry
// recompilation (compile+release churn) vs. just cheap redraws (render
// calls only). Numbers reflect the most recently rendered frame.
void rmlui_render_stats_debug_menu() {
	const RmlUiRenderStats& s = g_rmlui_render_stats;
	ImGui::Text("Geometry compiles: %d", s.compile_geometry_calls);
	ImGui::Text("  of which new GPU objects: %d", s.gpu_objects_created);
	ImGui::Text("Geometry releases: %d", s.release_geometry_calls);
	ImGui::Text("Draw calls (RenderGeometry): %d", s.render_geometry_calls);
	ImGui::Text("Vertex bytes uploaded: %zu", s.vertex_bytes_uploaded);
	ImGui::Text("Index bytes uploaded: %zu", s.index_bytes_uploaded);
	ImGui::Separator();
	ImGui::Text("Texture loads (LoadTexture): %d", s.load_texture_calls);
	ImGui::Text("Texture generates (GenerateTexture): %d", s.generate_texture_calls);
}
ADD_TO_DEBUG_MENU(rmlui_render_stats_debug_menu);
