#pragma once
// Thin cross-module hook: OpenGlDevice::rmlui_render() calls this between
// its begin_frame/end_frame GL state setup to draw all active Rml::Context
// instances, without IGraphicsDevice/OpenGlDevice needing to include RmlUi
// headers or know about RmlUiSystem. RmlUiSystem installs the callback at
// startup and clears it at shutdown.
#include <functional>

extern std::function<void()> g_rmlui_render_contexts;
