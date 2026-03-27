// Source/IntegrationTests/Screenshot.cpp
#include "Screenshot.h"
#include "External/glad/glad.h"
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

static std::string actual_path(const char* name) {
	return std::string("TestFiles/screenshots/") + name + "_actual.png";
}
static std::string golden_path(const char* name) {
	return std::string("TestFiles/goldens/") + name + ".png";
}

bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg) {
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	int w = vp[2], h = vp[3];

	std::vector<unsigned char> pixels(w * h * 3);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// GL gives bottom-left origin — flip rows for PNG top-left convention
	std::vector<unsigned char> flipped(pixels.size());
	int row_bytes = w * 3;
	for (int y = 0; y < h; ++y)
		memcpy(flipped.data() + y * row_bytes, pixels.data() + (h - 1 - y) * row_bytes, row_bytes);

	_mkdir("TestFiles");
	_mkdir("TestFiles/screenshots");

	std::string ap = actual_path(name);
	stbi_write_png(ap.c_str(), w, h, 3, flipped.data(), row_bytes);
	printf("  Screenshot saved: %s\n", ap.c_str());

	std::string gp = golden_path(name);

	if (cfg.promote) {
		_mkdir("TestFiles/goldens");
		if (stbi_write_png(gp.c_str(), w, h, 3, flipped.data(), row_bytes))
			printf("  [PROMOTE] Golden updated: %s\n", gp.c_str());
		else
			fprintf(stderr, "  [PROMOTE] Failed to write golden: %s\n", gp.c_str());
		return true;
	}

	int gw, gh, gc;
	unsigned char* golden = stbi_load(gp.c_str(), &gw, &gh, &gc, 3);
	if (!golden) {
		fprintf(stderr, "  SCREENSHOT FAIL: no golden at %s — run with --promote\n", gp.c_str());
		return false;
	}

	if (gw != w || gh != h) {
		fprintf(stderr, "  SCREENSHOT FAIL: size mismatch — golden %dx%d vs actual %dx%d\n", gw, gh, w, h);
		stbi_image_free(golden);
		return false;
	}

	int total = w * h;
	int diff_pixels = 0;
	int max_delta = 0;
	for (int i = 0; i < total * 3; ++i) {
		int d = abs((int)flipped[i] - (int)golden[i]);
		if (d > max_delta)
			max_delta = d;
		if (d > cfg.max_channel_delta)
			++diff_pixels;
	}
	stbi_image_free(golden);

	float diff_frac = (float)diff_pixels / (float)(total * 3);
	bool pass = (max_delta <= cfg.max_channel_delta) && (diff_frac <= cfg.max_diff_fraction);

	if (!pass)
		fprintf(stderr, "  SCREENSHOT FAIL: max_delta=%d diff_pixels=%d (%.4f%%) — golden: %s\n", max_delta,
				diff_pixels, diff_frac * 100.f, gp.c_str());
	else
		printf("  Screenshot OK: max_delta=%d diff_pixels=%d\n", max_delta, diff_pixels);
	return pass;
}
