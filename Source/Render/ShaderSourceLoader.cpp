// GLSL source loader. Lifted from OpenGlShaderImpl.cpp anon namespace in
// Phase 3.1 so both the OpenGL backend and the SDL3 GPU SPIR-V compile path
// can share the same #include-resolver + line-number tracker. No GL, no
// SPIR-V — pure text.

#include "ShaderSourceLoader.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"
#include <fstream>
#include <sstream>

namespace {

constexpr const char* SHADER_PATH = "Shaders\\";
constexpr const char* INCLUDE_SPECIFIER = "#include";

bool read_and_add_recursive(std::string filepath, ShaderSource& text, bool path_is_relative) {
	std::string path = path_is_relative ? SHADER_PATH : "";
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		sys_print(Error, "ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}

	ShaderSource::FileAndRange cur_range;
	cur_range.filename = filepath;
	cur_range.line_start = text.line_count;
	cur_range.input_line_start = 0;

	std::string line;
	while (std::getline(infile, line)) {
		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos) {
			text.source.append(line + '\n');
		} else if (pos == 0) {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				sys_print(Error, "ERROR: include not followed with filepath\n");
				return false;
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				sys_print(Error, "ERROR: include missing ending quote\n");
				return false;
			}

			cur_range.line_count = text.line_count - cur_range.line_start;
			text.ranges.push_back(cur_range);
			const int pre_start = cur_range.input_line_start + cur_range.line_count;
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text,
								   true /* so includes inside _glsl files work*/);

			cur_range.input_line_start = pre_start + 1;
			cur_range.line_start = text.line_count;
		}
		text.line_count++;
	}
	cur_range.line_count = text.line_count - cur_range.line_start;
	text.ranges.push_back(cur_range);

	return true;
}

int count_characters(const std::string& str, char ch) {
	int s = 0;
	for (auto c : str)
		s += int(c == ch);
	return s;
}

} // namespace

void ShaderSource::print_error(const std::string& driver_error) const {
	if (driver_error.find("ERROR: 0:") != 0)
		return;
	try {
		auto sub = driver_error.substr(9);
		auto split = StringUtils::split(sub);
		auto first = split.at(0);
		int line_num = std::stoi(first.substr(0, first.size() - 1));

		int best_fit = -1;
		for (int i = 0; i < (int)ranges.size(); i++) {
			int s = ranges[i].line_start;
			int c = ranges[i].line_count;
			if (line_num >= s && line_num < s + c) {
				best_fit = i;
				break;
			}
		}
		if (best_fit != -1) {
			auto& r = ranges[best_fit];
			const int input_start = r.input_line_start;
			const int ofs = line_num - r.line_start;
			const int line_in_source = ofs + input_start;
			sys_print(Error, "shader error in %s on line %d\n", r.filename.c_str(), line_in_source);
		}
	} catch (...) {
	}
}

std::string format_shader_defines(const std::string& comma_list) {
	std::stringstream ss(comma_list);
	std::string temp_define;
	std::string defines_with_directive;
	while (std::getline(ss, temp_define, ',') && temp_define.size() > 0) {
		defines_with_directive.append("#define " + temp_define + '\n');
	}
	return defines_with_directive;
}

ShaderSource load_shader_source(const std::string& path,
								const std::string& defines,
								bool path_is_relative,
								const char* version_prefix) {
	ShaderSource source;
	if (version_prefix && *version_prefix) {
		source.source = version_prefix;
	}
	source.source += defines;
	source.line_count += count_characters(source.source, '\n');

	bool result = read_and_add_recursive(path, source, path_is_relative);
	if (!result)
		return {};
	return source;
}
