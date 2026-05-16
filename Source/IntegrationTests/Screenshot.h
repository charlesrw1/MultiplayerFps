// Source/IntegrationTests/Screenshot.h
#pragma once
#include <string>
#include "glm/glm.hpp"
struct ScreenshotConfig
{
	bool promote = false;			  // write actual as golden instead of diffing
	bool interactive = false;		  // pause on failure
	int max_channel_delta = 8;		  // per-channel abs diff threshold (strict)
	float max_diff_fraction = 0.001f; // fraction of pixels allowed to differ (strict)
	// Soft-fail band: exceeding max_* but staying under warn_* prints a warning
	// and the screenshot still passes. Default = strict (warn_* == max_*), so the
	// band is OFF unless a caller opts in (see TestContext::capture_screenshot's
	// optional overrides for the per-shot opt-in path).
	int warn_channel_delta = 8;
	float warn_diff_fraction = 0.001f;
};

// Called by GameTestRunner after a frame renders when screenshot_pending is set.
// Returns true if test passes (or promote mode).
bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg, glm::ivec2 screen_size);
