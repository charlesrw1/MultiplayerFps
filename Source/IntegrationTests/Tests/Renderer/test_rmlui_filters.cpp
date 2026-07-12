// Source/IntegrationTests/Tests/Renderer/test_rmlui_filters.cpp
//
// Visually verifies RmlUiRenderInterface's layer stack + `filter: blur()` /
// `filter: drop-shadow()` support (GH #22) actually renders instead of
// silently no-op'ing (RmlUi core parses/animates unsupported filters fine -
// only a screenshot diff catches a backend that quietly does nothing).
// ex_filters.rml lays out three identical boxes: plain, blurred, and
// drop-shadowed, side by side so a golden diff shows the difference clearly.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "UI/RmlUi/RmlUiSystem.h"

static TestTask test_rmlui_filters(TestContext& t) {
	eng->load_level("");

	t.require(RmlUiSystem::inst != nullptr, "RmlUiSystem initialized");
	RmlDocHandle doc = RmlUiSystem::inst->load_document("ui/examples/ex_filters.rml");
	t.require(doc != RML_INVALID_DOC, "ex_filters.rml loaded");
	RmlUiSystem::inst->show_document(doc);

	co_await t.wait_ticks(2);

	co_await t.capture_screenshot("rmlui_filters");

	RmlUiSystem::inst->close_document(doc);
}
GAME_TEST("renderer/rmlui_filters", 15.f, test_rmlui_filters);
