// Source/IntegrationTests/RenderDump.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "RenderDump.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <direct.h>
#include "External/glad/glad.h"
#include "External/stb_image_write.h"
#include "Render/DrawLocal.h"

// Reads a texture as RGBA float, normalizes to uint8, writes PNG.
// depth=true uses GL_DEPTH_COMPONENT instead of GL_RGBA.
static void dump_one_texture(IGraphicsTexture* tex, const char* path) {
	if (!tex)
		return;
	auto size = tex->get_size();
	const int w = size.x;
	const int h = size.y;
	if (w <= 0 || h <= 0)
		return;

	GraphicsTextureFormat fmt = tex->get_texture_format();
	bool is_depth = (fmt == GraphicsTextureFormat::depth16f || fmt == GraphicsTextureFormat::depth24f ||
					 fmt == GraphicsTextureFormat::depth32f || fmt == GraphicsTextureFormat::depth24stencil8);

	uint32_t handle = tex->get_internal_handle();
	if (handle == 0)
		return;

	if (is_depth) {
		std::vector<float> pixels(w * h);
		glGetTextureImage(handle, 0, GL_DEPTH_COMPONENT, GL_FLOAT, (int)(w * h * sizeof(float)), pixels.data());
		std::vector<unsigned char> out(w * h * 4);
		for (int i = 0; i < w * h; ++i) {
			unsigned char v = (unsigned char)(pixels[i] * 255.f);
			out[i * 4 + 0] = v;
			out[i * 4 + 1] = v;
			out[i * 4 + 2] = v;
			out[i * 4 + 3] = 255;
		}
		stbi_flip_vertically_on_write(true);
		stbi_write_png(path, w, h, 4, out.data(), w * 4);
	} else {
		std::vector<float> pixels(w * h * 4);
		glGetTextureImage(handle, 0, GL_RGBA, GL_FLOAT, (int)(w * h * 4 * sizeof(float)), pixels.data());
		std::vector<unsigned char> out(w * h * 4);
		// find max to normalize HDR targets
		float maxv = 1.f;
		for (int i = 0; i < w * h * 4; ++i)
			if (pixels[i] > maxv)
				maxv = pixels[i];
		float scale = (maxv > 1.f) ? (1.f / maxv) : 1.f;
		for (int i = 0; i < w * h * 4; ++i) {
			float v = pixels[i] * scale;
			if (v < 0.f) v = 0.f;
			if (v > 1.f) v = 1.f;
			out[i] = (unsigned char)(v * 255.f);
		}
		stbi_flip_vertically_on_write(true);
		stbi_write_png(path, w, h, 4, out.data(), w * 4);
	}
}

void dump_render_targets(const char* test_name) {
	std::string dir = std::string("TestFiles/debug/") + test_name;
	_mkdir("TestFiles");
	_mkdir("TestFiles/debug");
	_mkdir(dir.c_str());

	struct Entry {
		const char* name;
		IGraphicsTexture* tex;
	};
	Entry entries[] = {
		{"actual_output_composite", draw.tex.actual_output_composite},
		{"output_composite",        draw.tex.output_composite},
		{"scene_color",             draw.tex.scene_color},
		{"last_scene_color",        draw.tex.last_scene_color},
		{"scene_gbuffer0",          draw.tex.scene_gbuffer0},
		{"scene_gbuffer1",          draw.tex.scene_gbuffer1},
		{"scene_gbuffer2",          draw.tex.scene_gbuffer2},
		{"scene_depth",             draw.tex.scene_depth},
		{"scene_motion",            draw.tex.scene_motion},
		{"ddgi_accum",              draw.tex.ddgi_accum},
		{"reflection_accum",        draw.tex.reflection_accum},
	};

	for (auto& e : entries) {
		if (!e.tex)
			continue;
		std::string path = dir + "/" + e.name + ".png";
		dump_one_texture(e.tex, path.c_str());
		printf("  Render dump: %s\n", path.c_str());
	}
}
