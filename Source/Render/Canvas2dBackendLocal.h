#pragma once
#include <vector>
#include "Render/Canvas2dTypes.h"

class IGraphicsTexture;

// Render-thread backend: submits the batch list recorded by Canvas2d/Canvas2dRecorder.
// Parallel role to the old RenderWindowBackendLocal, but manages its own render passes
// (switching color target/depth per run of same-target batches) instead of assuming the
// caller already has one open -- Canvas2d supports drawing to arbitrary offscreen
// targets via set_target_texture(), not just the always-open main composite pass the
// legacy UI path relied on.
class Canvas2dBackendLocal
{
public:
	void update(std::vector<Canvas2dBatch> new_batches) { batches = std::move(new_batches); }

	// depth is Canvas2d's dedicated 2d depth texture (nullptr until Canvas2d::init()
	// creates it); main_window_render_target is whichever composite texture counts as
	// "the main window" this frame -- depth-tested batches only get a real depth
	// attachment when their target is that texture (see design doc: offscreen
	// depth_test targets are out of scope for v1).
	void set_depth_texture(IGraphicsTexture* depth) {
		depth_texture = depth;
	}

	// Must be called every frame before render() with whichever texture is
	// actually going to reach the screen this frame (post-process ping-pong
	// means that isn't always the same texture) -- batches with target==nullptr
	// (the common case, drawn via set_target_window()) land here.
	void set_main_window_target(IGraphicsTexture* target) {
		main_window_target = target;
	}

	void render();

private:
	std::vector<Canvas2dBatch> batches;
	IGraphicsTexture* depth_texture = nullptr;
	IGraphicsTexture* main_window_target = nullptr;
};
