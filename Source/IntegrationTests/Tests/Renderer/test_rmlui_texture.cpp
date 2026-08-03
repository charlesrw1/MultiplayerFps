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

