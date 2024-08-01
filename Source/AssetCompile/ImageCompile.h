#pragma once

#include "Render/Texture.h"

// explcit params
struct ImageCompileParams
{
	std::string input_file;
	const Texture* t = nullptr;	// if t is not null, then sources data from it
	std::string output_file;
	Texture_Format output_format{};
	bool force_compile = false;	// if true, then forces compile even if input is older than existing output
	bool srgb = false;
};

class ImageCompilier
{
public:
	// auto
	static bool compile(const std::string& file, Texture_Format default_output_fmt = Texture_Format::TEXFMT_RGB8_DXT1);
	// manual
	static bool compile_from_params(const ImageCompileParams& params);
};