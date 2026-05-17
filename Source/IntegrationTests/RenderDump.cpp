// Source/IntegrationTests/RenderDump.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "RenderDump.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <direct.h>
#include "External/stb_image_write.h"
#include "Render/DrawLocal.h"

// Reads a texture in its native format and writes a normalized RGBA8 PNG.
// Per-format dispatch routes the download through IGraphicsTexture::download
// (which uses the texture's native format) and then expands/normalizes the
// pixels into 4-channel uint8 for PNG output.
static void dump_one_texture(IGraphicsTexture* tex, const char* path) {
	if (!tex)
		return;
	auto size = tex->get_size();
	const int w = size.x;
	const int h = size.y;
	if (w <= 0 || h <= 0)
		return;

	GraphicsTextureFormat fmt = tex->get_texture_format();
	using gtf = GraphicsTextureFormat;
	const int px = w * h;
	std::vector<unsigned char> out(px * 4, 255);

	auto normalize_to_u8 = [&](const std::vector<float>& src, int channels) {
		float maxv = 1.f;
		for (float v : src) if (v > maxv) maxv = v;
		const float scale = (maxv > 1.f) ? (1.f / maxv) : 1.f;
		for (int i = 0; i < px; ++i) {
			for (int c = 0; c < 4; ++c) {
				float v = (c < channels) ? src[i * channels + c] * scale : (c == 3 ? 1.f : 0.f);
				if (v < 0.f) v = 0.f; if (v > 1.f) v = 1.f;
				out[i * 4 + c] = (unsigned char)(v * 255.f);
			}
		}
	};

	switch (fmt) {
	case gtf::depth16f:
	case gtf::depth24f:
	case gtf::depth32f:
	case gtf::depth24stencil8: {
		std::vector<float> pixels(px);
		tex->download(0, -1, pixels.data(), (int)(pixels.size() * sizeof(float)));
		for (int i = 0; i < px; ++i) {
			unsigned char v = (unsigned char)(pixels[i] * 255.f);
			out[i * 4 + 0] = v; out[i * 4 + 1] = v; out[i * 4 + 2] = v; out[i * 4 + 3] = 255;
		}
	} break;
	case gtf::rgba8: {
		std::vector<unsigned char> pixels(px * 4);
		tex->download(0, -1, pixels.data(), (int)pixels.size());
		memcpy(out.data(), pixels.data(), pixels.size());
	} break;
	case gtf::rgb8: {
		std::vector<unsigned char> pixels(px * 3);
		tex->download(0, -1, pixels.data(), (int)pixels.size());
		for (int i = 0; i < px; ++i) {
			out[i * 4 + 0] = pixels[i * 3 + 0];
			out[i * 4 + 1] = pixels[i * 3 + 1];
			out[i * 4 + 2] = pixels[i * 3 + 2];
			out[i * 4 + 3] = 255;
		}
	} break;
	case gtf::rgb16f:
	case gtf::r11f_g11f_b10f: {
		std::vector<float> pixels(px * 3);
		tex->download(0, -1, pixels.data(), (int)(pixels.size() * sizeof(float)));
		normalize_to_u8(pixels, 3);
	} break;
	case gtf::rgba16f: {
		std::vector<float> pixels(px * 4);
		tex->download(0, -1, pixels.data(), (int)(pixels.size() * sizeof(float)));
		normalize_to_u8(pixels, 4);
	} break;
	case gtf::rg16f:
	case gtf::rg32f: {
		std::vector<float> pixels(px * 2);
		tex->download(0, -1, pixels.data(), (int)(pixels.size() * sizeof(float)));
		normalize_to_u8(pixels, 2);
	} break;
	default:
		// Unsupported format — leave PNG zeroed (alpha=255 already set).
		break;
	}
	stbi_flip_vertically_on_write(true);
	stbi_write_png(path, w, h, 4, out.data(), w * 4);
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
