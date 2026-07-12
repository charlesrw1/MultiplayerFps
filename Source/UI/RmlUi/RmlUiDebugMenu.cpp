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
// One extra slot for the Lua data-model demo, selected via rmlui_example_index == rmlui_example_count.
constexpr int rmlui_lua_demo_index = rmlui_example_count;
const char* rmlui_lua_demo_name = "Lua data model demo";

int rmlui_example_index = -1; // -1 = nothing shown yet
RmlDocHandle rmlui_example_doc = RML_INVALID_DOC;

// ex_rmlui_lua_demo.rml has data-model="rmlui_lua_demo_model" and a
// data-event-click callback bound to a function field on that model's Lua
// table (Data/scripts/demo/rmlui_lua_demo.lua) - it must be opened via
// rmlui.contexts["main"]:OpenDataModel from Lua, not RmlUiSystem::
// load_document, which only knows how to LoadDocument() with no model.
// ScriptManager::reload_from_content runs a snippet directly against the
// engine's live lua_State, so the debug menu can drive the same
// rmlui_demo_open()/rmlui_demo_close() globals the "rmlui_demo" console
// command uses.
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
	if (rmlui_example_index == rmlui_lua_demo_index)
		rmlui_run_lua("rmlui_demo_close()");
	rmlui_example_index = index;
	if (index == rmlui_lua_demo_index) {
		rmlui_run_lua("rmlui_demo_open()");
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
	if (rmlui_example_index == rmlui_lua_demo_index)
		current_name = rmlui_lua_demo_name;
	else if (rmlui_example_index >= 0)
		current_name = rmlui_examples[rmlui_example_index].display_name;
	ImGui::Text("Current: %s", current_name);

	if (ImGui::Button("<< Prev")) {
		int next = (rmlui_example_index <= 0) ? rmlui_lua_demo_index : rmlui_example_index - 1;
		rmlui_show_example(next);
	}
	ImGui::SameLine();
	if (ImGui::Button("Next >>")) {
		int next = (rmlui_example_index < 0 || rmlui_example_index >= rmlui_lua_demo_index) ? 0 : rmlui_example_index + 1;
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
	if (ImGui::Selectable(rmlui_lua_demo_name, rmlui_example_index == rmlui_lua_demo_index))
		rmlui_show_example(rmlui_lua_demo_index);
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
