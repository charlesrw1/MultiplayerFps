// Source/IntegrationTests/Tests/Renderer/test_rmlui_texture.cpp
//
// Verifies RmlUiRenderInterface::LoadTexture actually routes through the
// engine's real Texture asset system (Texture::load), not a placeholder:
// opens the Source-style settings example, switches to its
// "Fun" tab (two <img> elements - a UI png and a compiled game .dds, see
// ex_source_settings.rml), and checks each <img>'s laid-out size (a failed
// LoadTexture returns 0x0 dimensions, which a screenshot diff alone could
// miss if the img just renders as invisible/zero-size) before also
// capturing a screenshot golden. Not using g_rmlui_render_stats.
// load_texture_calls here - per RmlUiRenderInterface.h it's reset every
// begin_frame() for the live debug-menu HUD, not a cumulative session
// counter, so it can't distinguish "loaded 2 frames ago" from "never
// loaded".

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "UI/RmlUi/RmlUiSystem.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementTabSet.h>

static TestTask test_rmlui_texture_load(TestContext& t) {
	eng->load_level("");

	t.require(RmlUiSystem::inst != nullptr, "RmlUiSystem initialized");
	RmlDocHandle doc = RmlUiSystem::inst->load_document("ui/examples/ex_source_settings.rml");
	t.require(doc != RML_INVALID_DOC, "ex_source_settings.rml loaded");
	RmlUiSystem::inst->show_document(doc);

	co_await t.wait_ticks(2);

	Rml::ElementDocument* rdoc = RmlUiSystem::inst->get_document(doc);
	t.require(rdoc != nullptr, "document handle resolves");
	auto* tabset = rdoc->GetElementById("options_tabs");
	t.require(tabset != nullptr, "options_tabs element found");
	auto* as_tabset = rmlui_dynamic_cast<Rml::ElementTabSet*>(tabset);
	t.require(as_tabset != nullptr, "options_tabs is an ElementTabSet");

	as_tabset->SetActiveTab(6); // Fun is the 7th tab (0-based index 6)

	co_await t.wait_ticks(2);

	Rml::ElementList portraits;
	rdoc->QuerySelectorAll(portraits, ".npc-portrait");
	t.require(portraits.size() == 2, "both test <img> elements found");
	for (Rml::Element* img : portraits) {
		t.check(img->GetClientWidth() > 0.f && img->GetClientHeight() > 0.f,
			"img has nonzero laid-out size (LoadTexture returned real dimensions, not 0x0 failure)");
	}

	co_await t.capture_screenshot("rmlui_fun_tab_texture");

	RmlUiSystem::inst->close_document(doc);
}
GAME_TEST("renderer/rmlui_fun_tab_texture", 15.f, test_rmlui_texture_load);
