#include "MaterialLocal.h"
#include "Framework/Files.h"

#include "Framework/DictParser.h"
#include "AssetCompile/Someutils.h"

#include "Texture.h"

#include <algorithm>

CLASS_IMPL(MasterMaterial);
CLASS_IMPL(MaterialInstance);
CLASS_IMPL(MaterialParameterBuffer);

inline std::string remove_filename_from_path(std::string& path)
{
	auto find = path.rfind("/");
	if (find == std::string::npos) {
		return "";
	}
	else {
		std::string wo_filename = path.substr(0, find);
		path = path.substr(find);
		return wo_filename;
	}
}

MasterMaterialLocal* MaterialManagerLocal::find_master_material(const std::string& mastername)
{
	
	return {};
}

static const char* const MATERIAL_DIR = "./Data/Materials/";


#include <stdexcept>

#include <fstream>
class MasterMaterialExcept : public std::runtime_error
{
public:
	MasterMaterialExcept(const std::string& path, const std::string& error) 
		: std::runtime_error("!!! load MasterMaterial " + path + "error: " + error + "\n") {}
};
bool MasterMaterialLocal::load_from_file(const std::string& filename)
{
	DictParser in;
	std::string fullpath = MATERIAL_DIR + filename;
	auto file = FileSys::open_read(fullpath.c_str());
	if (!file) {
		sys_print("!!! no file for master materila %s\n", fullpath.c_str());
		return false;
	}
	in.load_from_file(file.get());
	
	std::string vs_code;
	std::string fs_code;
	std::vector<InstanceData> inst_dats;

	StringView tok;
	if (!in.read_string(tok) || !tok.cmp("TYPE") || !in.read_string(tok) || !tok.cmp("MaterialMaster")) {
		throw MasterMaterialExcept(fullpath, "Expceted TYPE MaterialMaster");
	}
	auto parse_options = [&](const std::vector<std::string>& opts)->int {
		in.read_string(tok);
		for (int i = 0; i < opts.size(); i++) {
			if (tok.cmp(opts[i].c_str()))
				return i;
		}
		throw MasterMaterialExcept(fullpath, "Unknown option " + to_std_string_sv(tok));
	};
	while (in.read_string(tok) && !in.is_eof()) {
		if (tok.cmp("VAR")) {
			MaterialParameterDefinition def;
			def.default_value.type = (MatParamType)parse_options({ "_null_","float_vec4","float","vec4","bool","texture2D","constTexture2D" });
			in.read_string(tok);
			def.name = to_std_string_sv(tok);
			
			switch (def.default_value.type)
			{
			case MatParamType::Bool: {
				int b = 0; in.read_int(b); def.default_value.boolean = b;
			}break;
			case MatParamType::Float: {
				float f = 0.0;
				in.read_float(f);
				def.default_value.scalar = f;
			}break;
			case MatParamType::Vector: {
				int r, g, b, a;
				in.read_int(r);
				in.read_int(g);
				in.read_int(b);
				in.read_int(a);
				Color32 c;
				c.r = r;
				c.g = g;
				c.b = b;
				c.a = a;
				def.default_value.color32 = c.to_uint();
			}break;
			case MatParamType::FloatVec: {
				glm::vec4 v;
				in.read_float(v.x);
				in.read_float(v.y);
				in.read_float(v.z);
				in.read_float(v.w);
				def.default_value.vector = v;
			}break;
			case MatParamType::Texture2D:
			case MatParamType::ConstTexture2D:
			{
				in.read_string(tok);
				def.default_value.tex_ptr = g_imgs.find_texture(to_std_string_sv(tok).c_str());
			}break;

			default:
				throw MasterMaterialExcept(fullpath, "bad VAR type");
				break;
			}

			param_defs.push_back(def);
		}
		else if (tok.cmp("OPT")) {

			in.read_string(tok);

			if (tok.cmp("AlphaTested")) {
				alpha_tested = parse_options({ "false","true" });
			}
			else if (tok.cmp("BlendMode")) {
				blend = (blend_state)parse_options({ "Opaque","Blend","Add"});
			}
			else if (tok.cmp("LightingMode")) {
				light_mode = (LightingMode)parse_options({ "Lit","Unlit" });
			}
			else if (tok.cmp("ShowBackfaces")) {
				backface = parse_options({ "false","true" });
			}
			else
				throw MasterMaterialExcept(fullpath, "Unknown OPT " + to_std_string_sv(tok));
		}
		else if (tok.cmp("UBO")) {
			in.read_string(tok);

			ASSERT(0);
		}
		else if (tok.cmp("INST")) {
			in.read_string(tok);
			InstanceData id;
			id.is_vector_type = parse_options({ "float","vec4" });

			in.read_string(tok);
			id.name = to_std_string_sv(tok);
			int index = 0;
			in.read_int(index);
			if (index < 0 || index >= MAX_INSTANCE_PARAMETERS) {
				throw MasterMaterialExcept(fullpath, "INST index not valid " + std::to_string(index));
			}
			inst_dats.push_back(id);
		}
		else if (tok.cmp("_VS_BEGIN")) {
			while (in.read_line(tok)) {
				std::string line = to_std_string_sv(tok);
				if (line.find("_VS_END")!=std::string::npos) {
					break;
				}
				vs_code += line;
			}
		}
		else if (tok.cmp("_FS_BEGIN")) {
			while (in.read_line(tok)) {
				std::string line = to_std_string_sv(tok);
				if (line.find("_FS_END") != std::string::npos) {
					break;
				}
				fs_code += to_std_string_sv(tok);
			}
		}
		else {
			throw MasterMaterialExcept(fullpath, "Unknown cmd : " + to_std_string_sv(tok));
		}
	}


	std::sort(param_defs.begin(), param_defs.end(), 
		[](const auto& a, const auto& b) -> bool {
			return (int)a.default_value.type < (int)b.default_value.type;
		}
	);

	uint32_t param_ofs = 0;
	uint32_t tex_ofs = 0;

	for (int i = 0; i < param_defs.size(); i++) {
		auto& pd = param_defs[i];
		switch (pd.default_value.type)
		{
		case MatParamType::FloatVec:
			pd.offset = param_ofs;
			param_ofs += 16;	// sizeof(float)*4
			break;
		case MatParamType::Float:
		case MatParamType::Vector:
			pd.offset = param_ofs;
			param_ofs += 4; // sizeof(float) or sizeof(color32)
			break;
		case MatParamType::Bool:
			pd.offset = param_ofs;
			pd.offset += 1;
			break;
		case MatParamType::Texture2D:
		case MatParamType::ConstTexture2D:
			pd.offset = tex_ofs++;
			break;
		default:
			break;
		}
	}

	if (param_ofs > MATERIAL_SIZE) {
		throw MasterMaterialExcept(fullpath, "Too many material parameters exceeds max material size of 64 bytes");
	}

	auto str = create_glsl_shader(vs_code, fs_code, inst_dats);
	std::ofstream outfile("file.txt");
	outfile.write(str.data(), str.size());
}


static const char* const SHADER_PATH = "Shaders\\";
static const char* const INCLUDE_SPECIFIER = "#include";


static bool read_and_add_recursive(std::string filepath, std::string& text)
{
	std::string path(SHADER_PATH);
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		printf("ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}


	std::string line;
	while (std::getline(infile, line)) {

		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				printf("ERROR: include not followed with filepath\n");
				return false;
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				printf("ERROR: include missing ending quote\n");
				return false;
			}
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text);

		}
	}

	return true;
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

std::string MasterMaterialLocal::create_glsl_shader(
	std::string& vs_code,
	std::string& fs_code,
	const std::vector<InstanceData>& instdat
)
{
	std::string masterShader;
	bool good = read_and_add_recursive("MasterShader.txt", masterShader);
	if (!good)
		throw MasterMaterialExcept("...", "couldnt open MasterShader.txt");

	std::string actual_vs_code;

	auto autogen_code = [&](std::string& actual_code, std::string& inp_code) {

		actual_code += "// Texture defs\n";
		for (int i = 0; i < param_defs.size(); i++) {
			if (param_defs[i].default_value.type == MatParamType::Texture2D ||
				param_defs[i].default_value.type == MatParamType::ConstTexture2D) {
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
			if (type == MatParamType::Texture2D || type == MatParamType::ConstTexture2D)
				continue;

			const uint32_t UINT_OFS = def.offset / 4;
			const uint32_t BYTE_OFS = def.offset % 4;

			std::string replacement_code;
			switch (type) {
			case MatParamType::Float:
				replacement_code = "uintBitsToFloat( g_materials[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::Vector:
				replacement_code = "unpackUnorm4x8( g_materials[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::FloatVec:
				replacement_code = "vec4( ";
				replacement_code += "uintBitsToFloat(g_materials[FS_IN_Matid + " + std::to_string(UINT_OFS) + "] ), ";
				replacement_code += "uintBitsToFloat(g_materials[FS_IN_Matid + " + std::to_string(UINT_OFS + 1) + "] ), ";
				replacement_code += "uintBitsToFloat(g_materials[FS_IN_Matid + " + std::to_string(UINT_OFS + 2) + "] ), ";
				replacement_code += "uintBitsToFloat(g_materials[FS_IN_Matid + " + std::to_string(UINT_OFS + 3) + "] )";
				replacement_code += ")";
				break;
			}
			replacement_code += " /* ";
			replacement_code += def.name;
			replacement_code += " */ ";

			replace(inp_code, def.name, replacement_code);
		}
		actual_code += inp_code;

	};

	if (!vs_code.empty()) {
		autogen_code(actual_vs_code, vs_code);
	}
	else
		actual_vs_code = "void VSmain() { }\n";

	std::string actual_fs_code;
	if (!fs_code.empty()) {
		autogen_code(actual_fs_code, fs_code);
	}
	else
		actual_fs_code = "void FSmain() { }\n";


	replace(masterShader, "___USER_VS_CODE___", actual_vs_code);
	replace(masterShader, "___USER_FS_CODE___", actual_fs_code);

	return masterShader;
}