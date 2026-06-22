#include "MaterialLocal.h"
#include "Framework/StringUtils.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef EDITOR_BUILD

// forward declaration — defined in MaterialLocal.cpp
const char* get_master_shader_path(MaterialUsage usage);

static void read_and_add_recursive(std::string filepath, std::string& text);
static void read_instream(std::istream& stream, std::string& text) {

	static const char* const INCLUDE_SPECIFIER = "#include";

	std::string line;
	while (std::getline(stream, line)) {

		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else if (pos == 0) {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				throw MasterMaterialExcept("include not followed with filepath\n");
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				throw MasterMaterialExcept("include missing ending quote\n");
			}
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text);
		}
	}
}

static void read_and_add_recursive(std::string filepath, std::string& text) {
	static const char* const SHADER_PATH = "Shaders\\";
	std::string path(SHADER_PATH);
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		throw MasterMaterialExcept("couldn't open shader path" + path);
	}
	read_instream(infile, text);
}
static void expand_includes_of_text(std::string& inouttext) {
	std::stringstream ss;
	ss << inouttext;
	inouttext.clear();
	read_instream(ss, inouttext);
}

static void replace(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

static void replace_variable(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		// check the characters before and after
		if (start_pos > 0 && isalnum(str.at(start_pos - 1))) {
			start_pos += from.size();
			continue;
		}
		if (start_pos + from.size() < str.size() - 1 && isalnum(str.at(start_pos + from.size()))) {
			start_pos += from.size();
			continue;
		}
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

std::string MasterMaterialImpl::create_glsl_shader(std::string& vs_code, std::string& fs_code,
												   const std::vector<InstanceData>& instdat) {
	std::string masterShader;

	const char* master_shader_path = get_master_shader_path(usage);

	// handle defines here?
	if (usage == MaterialUsage::Decal) {
		if (decal_affect_albedo)
			masterShader += "#define DECAL_ALBEDO_WRITE\n";
		if (decal_affect_emissive)
			masterShader += "#define DECAL_EMISSIVE_WRITE\n";
		if (decal_affect_normal)
			masterShader += "#define DECAL_NORMAL_WRITE\n";
		if (decal_affect_roughmetal)
			masterShader += "#define DECAL_ROUGHMETAL_WRITE\n";
	}
	if (is_alphatested()) {
		masterShader += "#define ALPHATEST\n";
	}
	if (blend != BlendState::OPAQUE) {
		masterShader += "#define FORWARD_SHADER\n";
		if (light_mode != LightingMode::Unlit) {
			masterShader += "#define FORWARD_LIT\n";
		}
	}

	try {
		read_and_add_recursive(master_shader_path, masterShader);
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("in master shader %s: %s", master_shader_path, m.what()));
	}

	std::string actual_vs_code;

	auto autogen_code = [&](const char* for_what, std::string& actual_code, std::string& inp_code) {
		actual_code += "// Texture defs\n";
		for (int i = 0; i < param_defs.size(); i++) {
			if (param_defs[i].default_value.type == MatParamType::Texture2D) {
				int index = param_defs[i].offset;
				ASSERT(index >= 0 && index < 32);

				// layout(binding=0) uniform sampler2D exampleTexture;
				actual_code += "layout(binding = ";
				actual_code += std::to_string(index);
				actual_code += " ) uniform sampler2D ";
				actual_code += param_defs[i].name;
				actual_code += ";\n";
			}
		}
		actual_code += "\n";

		// replace any parameter data references
		for (int i = 0; i < param_defs.size(); i++) {
			auto& def = param_defs[i];
			auto type = def.default_value.type;
			if (type == MatParamType::Texture2D)
				continue;

			const uint32_t UINT_OFS = def.offset / 4;
			const uint32_t BYTE_OFS = def.offset % 4;

			std::string replacement_code;
			switch (type) {
			case MatParamType::Float:
				replacement_code = "uintBitsToFloat( _material_param_buffer[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::Vector:
				replacement_code = "unpackUnorm4x8( _material_param_buffer[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::FloatVec:
				replacement_code = "vec4( ";
				replacement_code +=
					"uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS) + "] ), ";
				replacement_code +=
					"uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 1) + "] ), ";
				replacement_code +=
					"uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 2) + "] ), ";
				replacement_code +=
					"uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 3) + "] )";
				replacement_code += ")";
				break;
			}
			replacement_code += " /* ";
			replacement_code += def.name;
			replacement_code += " */ ";

			replace_variable(inp_code, def.name, replacement_code);
		}
		actual_code += "\n";
		actual_code += inp_code;
		try {
			expand_includes_of_text(actual_code);
		}
		catch (MasterMaterialExcept m) {
			throw MasterMaterialExcept(string_format("during codegen %s: %s", for_what, m.what()));
		}
	};

	if (!vs_code.empty()) {
		autogen_code("VS", actual_vs_code, vs_code);
	} else
		actual_vs_code = "void VSmain() { }\n";

	std::string actual_fs_code;
	actual_fs_code += "const uint _MATERIAL_TYPE = ";
	// names defined in SharedGpuTypes.txt
	switch (light_mode) {
	case LightingMode::Lit:          actual_fs_code += "MATERIAL_TYPE_LIT;\n"; break;
	case LightingMode::Unlit:        actual_fs_code += "MATERIAL_TYPE_UNLIT;\n"; break;
	case LightingMode::Clearcoat:    actual_fs_code += "MATERIAL_TYPE_CLEARCOAT;\n"; break;
	case LightingMode::Iridescence:  actual_fs_code += "MATERIAL_TYPE_IRIDESCENCE;\n"; break;
	case LightingMode::Sheen:        actual_fs_code += "MATERIAL_TYPE_SHEEN;\n"; break;
	case LightingMode::Subsurface:   actual_fs_code += "MATERIAL_TYPE_SUBSURFACE;\n"; break;
	case LightingMode::Translucent:  actual_fs_code += "MATERIAL_TYPE_TRANSLUCENT;\n"; break;
	case LightingMode::Anisotropic:  actual_fs_code += "MATERIAL_TYPE_ANISOTROPIC;\n"; break;
	case LightingMode::Hair:         actual_fs_code += "MATERIAL_TYPE_HAIR;\n"; break;
	}

	if (!fs_code.empty()) {
		autogen_code("FS", actual_fs_code, fs_code);
	} else
		actual_fs_code += "void FSmain() { }\n";

	replace(masterShader, "___USER_VS_CODE___", actual_vs_code);
	replace(masterShader, "___USER_FS_CODE___", actual_fs_code);

	masterShader.insert(0, "// ***********************************\n"
						   "// **** GENERATED MATERIAL SHADER ****\n"
						   "// ***********************************\n");

	return masterShader;
}
#endif
