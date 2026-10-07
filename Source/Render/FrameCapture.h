#pragma once

#include <string>
#include "glm/glm.hpp"

// Downloads the most recent composited frame and writes it as a PNG. Must be called on the
// engine thread while the renderer is initialized.
bool capture_composite_frame_png(const std::string& path, glm::ivec2& out_size, std::string& out_error);
