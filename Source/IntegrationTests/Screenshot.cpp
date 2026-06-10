// Source/IntegrationTests/Screenshot.cpp
#include "Screenshot.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <direct.h> // _mkdir on Windows

// STB_IMAGE_IMPLEMENTATION and STB_IMAGE_WRITE_IMPLEMENTATION are already defined
// in Source/External/External.cpp — do NOT redefine them here.
#include "External/stb_image.h"
#include "External/stb_image_write.h"

#include "glm/glm.hpp"
#include "Render/DrawLocal.h"
#include "RenderDump.h"
static std::string actual_path(const char* name) {
	return std::string("TestFiles/screenshots/") + name + "_actual.png";
}
static std::string golden_path(const char* name) {
	return std::string("TestFiles/goldens/") + name + ".png";
}

bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg, glm::ivec2 _unused_) {
	IGraphicsTexture* tex = draw.tex.actual_output_composite;
	auto size_to_use = tex->get_size();
	const int w = size_to_use.x;
	const int h = size_to_use.y;

	// Native format is rgb8 (3 channels). Download then expand to RGBA so the
	// rest of this routine + golden goldens stay in RGBA8 land.
	std::vector<unsigned char> rgb(w * h * 3);
	tex->download(0, -1, rgb.data(), (int)rgb.size());
	std::vector<unsigned char> pixels(w * h * 4);
	for (int i = 0; i < w * h; ++i) {
		pixels[i * 4 + 0] = rgb[i * 3 + 0];
		pixels[i * 4 + 1] = rgb[i * 3 + 1];
		pixels[i * 4 + 2] = rgb[i * 3 + 2];
		pixels[i * 4 + 3] = 255;
	}
	const int stride = w * 4;


	_mkdir("TestFiles");
	_mkdir("TestFiles/screenshots");

	std::string ap = actual_path(name);
	stbi_flip_vertically_on_write(true);
	stbi_write_png(ap.c_str(), w, h, 4, pixels.data(), stride);
	printf("  Screenshot saved: %s\n", ap.c_str());

	std::string gp = golden_path(name);

	if (cfg.promote) {
		_mkdir("TestFiles/goldens");
		if (stbi_write_png(gp.c_str(), w, h, 4, pixels.data(), stride))
			printf("  [PROMOTE] Golden updated: %s\n", gp.c_str());
		else
			fprintf(stderr, "  [PROMOTE] Failed to write golden: %s\n", gp.c_str());
		return true;
	}

	int gw, gh, gc;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* golden = stbi_load(gp.c_str(), &gw, &gh, &gc, 4);
	if (!golden) {
		fprintf(stderr, "  SCREENSHOT FAIL: no golden at %s — run with --promote\n", gp.c_str());
		// SKIP THESE
		//dump_render_targets(name);
		return false;
	}

	if (gw != w || gh != h) {
		fprintf(stderr, "  SCREENSHOT FAIL: size mismatch — golden %dx%d vs actual %dx%d\n", gw, gh, w, h);
		stbi_image_free(golden);
		// SKIP THESE
		//dump_render_targets(name);
		return false;
	}

	int total = w * h;
	int diff_pixels = 0;
	int max_delta = 0;
	for (int i = 0; i < total * 4; ++i) {
		int d = abs((int)pixels[i] - (int)golden[i]);
		if (d > max_delta)
			max_delta = d;
		if (d > cfg.max_channel_delta)
			++diff_pixels;
	}
	stbi_image_free(golden);

	float diff_frac = (float)diff_pixels / (float)(total * 3);
	const bool within_strict = (max_delta <= cfg.max_channel_delta) && (diff_frac <= cfg.max_diff_fraction);
	const bool within_warn   = (max_delta <= cfg.warn_channel_delta) && (diff_frac <= cfg.warn_diff_fraction);

	if (within_strict) {
		printf("  Screenshot OK: max_delta=%d diff_pixels=%d\n", max_delta, diff_pixels);
		return true;
	}
	if (within_warn) {
		// Soft-fail: minor drift (likely GPU/driver rounding) — surface it so it
		// doesn't go unnoticed, but don't fail the test. Use stdout, not stderr:
		// PowerShell 5.1 renders any stderr line from a native exe as a red
		// NativeCommandError, which looks like a failure even though this passes.
		printf(
				"  SCREENSHOT WARN: max_delta=%d diff_pixels=%d (%.4f%%) — within warn band (delta<=%d, frac<=%.4f%%), golden: %s\n",
				max_delta, diff_pixels, diff_frac * 100.f, cfg.warn_channel_delta,
				cfg.warn_diff_fraction * 100.f, gp.c_str());
		return true;
	}
	fprintf(stderr, "  SCREENSHOT FAIL: max_delta=%d diff_pixels=%d (%.4f%%) — golden: %s\n", max_delta,
			diff_pixels, diff_frac * 100.f, gp.c_str());
	// SKIP THESE
	//dump_render_targets(name);
	return false;
}
