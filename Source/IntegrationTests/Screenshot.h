// Source/IntegrationTests/Screenshot.h
#pragma once
#include <string>

struct ScreenshotConfig
{
	bool promote = false;			  // write actual as golden instead of diffing
	bool interactive = false;		  // pause on failure
	int max_channel_delta = 8;		  // per-channel abs diff threshold
	float max_diff_fraction = 0.001f; // fraction of pixels allowed to differ
};

// Called by GameTestRunner after a frame renders when screenshot_pending is set.
// Returns true if test passes (or promote mode).
bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg);
