// Backend-agnostic IGraphicsDevice global state: the active instance pointer,
// gfx() / gfx_is_initialized() / gfx_shutdown(), and the backend-select cvar.
// Split out from OpenGlDevice.cpp (P3.1 D1) so the DX11 backend doesn't need
// to link against the OpenGL TU to reach these.

#include "IGraphicsDevice.h"

IGraphicsDevice* g_gfx_instance = nullptr;

// Not CVAR_READONLY: that flag rejects set_string() unconditionally (see
// Config.cpp), which would prevent EngineVars.ini from ever overriding the
// "opengl" default. Read once at startup before window creation; setting it
// afterward has no effect (documented, not enforced).
ConfigVar g_render_backend("r.render_backend", "opengl", CVAR_DEV,
							"Render backend selected at startup: \"opengl\" or \"dx11\". Read before window creation; changing it afterward has no effect.");

IGraphicsDevice& gfx() {
	ASSERT(g_gfx_instance != nullptr);
	return *g_gfx_instance;
}
bool gfx_is_initialized() { return g_gfx_instance != nullptr; }

void gfx_shutdown() {
	if (g_gfx_instance) {
		g_gfx_instance->shutdown_backend();
		delete g_gfx_instance;
		g_gfx_instance = nullptr;
	}
}
