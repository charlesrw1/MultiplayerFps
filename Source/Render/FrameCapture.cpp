#include "FrameCapture.h"

#include "DrawLocal.h"
#include "External/stb_image_write.h"
#include <filesystem>
#include <vector>

bool capture_composite_frame_png(const std::string& path, glm::ivec2& out_size, std::string& out_error) {
	if (path.empty()) {
		out_error = "capture path cannot be empty";
		return false;
	}
	if (!draw.tex.actual_output_composite) {
		out_error = "the composite render target is not initialized";
		return false;
	}

	std::filesystem::path output = std::filesystem::absolute(path);
	std::error_code ec;
	if (output.has_parent_path()) {
		std::filesystem::create_directories(output.parent_path(), ec);
		if (ec) {
			out_error = "could not create output directory: " + ec.message();
			return false;
		}
	}

	IGraphicsTexture* texture = draw.tex.actual_output_composite;
	out_size = texture->get_size();
	if (out_size.x <= 0 || out_size.y <= 0) {
		out_error = "the composite render target has invalid dimensions";
		return false;
	}

	std::vector<unsigned char> pixels((size_t)out_size.x * out_size.y * 3);
	texture->download(0, -1, pixels.data(), (int)pixels.size());
	stbi_flip_vertically_on_write(true);
	if (!stbi_write_png(output.string().c_str(), out_size.x, out_size.y, 3, pixels.data(), out_size.x * 3)) {
		out_error = "failed to write PNG: " + output.string();
		return false;
	}
	return true;
}
