// Debug menu that flips through a fixed set of self-contained RmlUi example
// documents (Data/ui/examples/*.rml) - lets you eyeball layout/text/controls
// without a level or Lua glue. See docs/ui/rmlui_agent_guide.md.
#include "RmlUiSystem.h"
#include "Framework/Config.h"
#include "imgui.h"

namespace {
struct RmlUiExample {
	const char* display_name;
	const char* doc_path; // relative to Data/, passed to RmlUiSystem::load_document
};

const RmlUiExample rmlui_examples[] = {
	{ "Typography", "ui/examples/ex_typography.rml" },
	{ "Flexbox layout", "ui/examples/ex_flexbox.rml" },
	{ "Controls", "ui/examples/ex_controls.rml" },
	{ "Compass HUD", "ui/examples/ex_compass.rml" },
};
constexpr int rmlui_example_count = sizeof(rmlui_examples) / sizeof(rmlui_examples[0]);

int rmlui_example_index = -1; // -1 = nothing shown yet
RmlDocHandle rmlui_example_doc = RML_INVALID_DOC;

void rmlui_show_example(int index) {
	if (!RmlUiSystem::inst)
		return;
	if (rmlui_example_doc != RML_INVALID_DOC) {
		RmlUiSystem::inst->close_document(rmlui_example_doc);
		rmlui_example_doc = RML_INVALID_DOC;
	}
	rmlui_example_index = index;
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

	const char* current_name = (rmlui_example_index >= 0) ? rmlui_examples[rmlui_example_index].display_name : "(none)";
	ImGui::Text("Current: %s", current_name);

	if (ImGui::Button("<< Prev")) {
		int next = (rmlui_example_index <= 0) ? rmlui_example_count - 1 : rmlui_example_index - 1;
		rmlui_show_example(next);
	}
	ImGui::SameLine();
	if (ImGui::Button("Next >>")) {
		int next = (rmlui_example_index < 0 || rmlui_example_index >= rmlui_example_count - 1) ? 0 : rmlui_example_index + 1;
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
}
ADD_TO_DEBUG_MENU(rmlui_examples_debug_menu);
