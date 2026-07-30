#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/LuaColor.h"
#include "Framework/Rect2d.h"
#include "Render/DrawTypedefs.h"
#include "UI/BaseGUI.h" // guiAnchor -- moves to UI/UiAnchor.h in the FontAsset-rename pass
#include <glm/glm.hpp>

class Texture;
class GuiFont; // renamed FontAsset in a later build-order step
class MaterialInstance;
class Canvas2dVertexArray;
struct DrawSettings;
class IGraphicsTexture;

// Immediate-mode *recording* 2d drawing API: draw_* calls append geometry + append/extend
// a batch (see Canvas2dRecorder), nothing touches the GPU until sync_to_renderer(). See
// the Canvas2d design doc header comment further down this file for the coordinate-space
// and per-frame-reset conventions every draw call relies on.
//
// Coordinates are pixels in the current target's space, origin top-left, Y-down (matches
// the orthographic projection set up per-frame in update()). All recording state
// (target/viewport/view matrix/scissor stack) resets to defaults at the start of every
// frame in update(), before any game-update code runs -- nothing recorded one frame leaks
// into the next. Canvas2d:: calls are only valid from game-update code (main thread,
// during the tick that gets flushed that frame), not from the render thread and not
// after this frame's sync_to_renderer().
class Canvas2d : public ClassBase
{
public:
	CLASS_BODY(Canvas2d);

	// non-REF, C++-only lifecycle
	static void init();				  // one-time asset/depth-texture setup (ordering-dependent, see design doc)
	static void update();				  // per-frame: reset recording state to defaults
	static void sync_to_renderer();	  // hand the recorded batch list to the render-thread backend
	static void on_resize(int w, int h); // recreate the dedicated 2d depth texture

	REF static void set_target_window();
	REF static void set_target_texture(Texture* render_texture);
	REF static void set_clear(bool clear_color, lColor color, bool clear_depth);
	REF static void set_viewport(int x, int y, int w, int h);
	static void set_view_matrix(glm::mat4 view); // non-REF, C++-only

	// Scissor: simple set/clear for basic use, plus a push/pop stack so nested draws
	// (e.g. a HUD panel clipping its children) can't leak state into later draws by
	// forgetting to restore it. push_scissor intersects with the current rect; pop_scissor
	// restores the prior rect (or "no scissor" if the stack is empty).
	REF static void set_scissor(int x, int y, int w, int h);
	REF static void clear_scissor();
	REF static void push_scissor(int x, int y, int w, int h);
	REF static void pop_scissor();

	REF static void draw_sprite(float x, float y, float w, float h, Texture* tex, lColor color);
	REF static void draw_sprite_ex(float x, float y, float w, float h, Texture* tex, lColor color, lRect uv_rect,
									BlendState blend, float z, glm::mat4 transform);
	// Lua-exposed REF functions can't carry C++ default argument values (the codegen
	// parser has no support for it) -- pass nullptr/guiAnchor::TopLeft explicitly for the
	// old defaults instead of omitting the args.
	REF static void draw_text(std::string text, int x, int y, lColor color, GuiFont* font, guiAnchor anchor);
	REF static lRect measure_text(std::string text, GuiFont* font);
	REF static lRect get_screen_size(); // always the main window, regardless of current target
	REF static lRect get_target_size(); // the currently active set_target_* target

	// Gui:: parity (migrated call sites keep these names, now with explicit color)
	REF static void rectangle(int x, int y, int w, int h, lColor color);
	REF static void rectangle_outline(int x, int y, int w, int h, int thickness, lColor color);
	REF static void circle(int x, int y, int radius, int segments, lColor color);
	REF static void line(int x1, int y1, int x2, int y2, int thickness, lColor color);

	static void draw_vertex_array(Canvas2dVertexArray& arr, int index_offset, int index_count,
								   const DrawSettings& settings, glm::mat4 transform = glm::mat4(1.f));

	// Canvas:: parity -- these have nothing to do with ViewportSystem's surviving
	// responsibilities, so they fold into Canvas2d instead
	REF static void set_window_fullscreen(bool is_fullscreen);
	REF static void set_window_title(std::string name);
	REF static void set_window_capture_mouse(bool capturing_mouse);

	static const GuiFont* get_default_font();
};
