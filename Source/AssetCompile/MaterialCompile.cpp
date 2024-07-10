#include "Compiliers.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Someutils.h"
#include <stdexcept>

#include "Render/MaterialLocal.h"

static const char* MATERIAL_DIRECTORY = "./Data/Materials/";

bool MaterialCompilier::compile(const char* name)
{
	sys_print("------ Compiling Material %s ------\n",name);

	std::string path = MATERIAL_DIRECTORY;
	path += name;
	path += ".txt";

	auto file = FileSys::open_read_os(path.c_str());
	if (!file) {
		sys_print("!!! couldn't compile material %s because OS file not found\n", name);
		return false;
	}

	std::vector<uint8_t> buffer;
	buffer.resize(file->size());
	file->read(buffer.data(), buffer.size());

	DictParser parser;
	parser.load_from_memory(buffer.data(), buffer.size(), name);
	StringView tok;
	parser.read_string(tok);
	if (!parser.expect_item_start())
		return false;

	// only care about images referenced
	while (parser.read_string(tok) && !parser.is_eof() && !parser.check_item_end(tok))
	{
		if (tok.cmp("albedo") || tok.cmp("normal") || tok.cmp("ao") || tok.cmp("rough") || tok.cmp("metal") || tok.cmp("special")) {
			parser.read_string(tok);
			std::string image_path = to_std_string_sv(tok);
			sys_print("*** compiling image %s\n", image_path.c_str());
		}
	}

	return true;
}

class MaterialDef
{
public:
	std::string vertex_shader_code;
	std::string fragment_shader_code;

	std::unordered_map<std::string, std::string> s;
	std::vector<std::string> global_buffers;

	struct InstanceDataDef {
		std::string name;
		MatParamType type=MatParamType::Float;
		int index=0;
	};
	std::vector<InstanceDataDef> instance_data;

	struct ConstTextures {
		std::string name;
		std::string path;
	};
	std::vector<ConstTextures> const_texs;

	bool alpha_tested = false;
	bool show_backfaces = false;
	blend_state blending = blend_state::OPAQUE;

	struct ParameterDef {
		std::string name;
		MatParamType type = MatParamType::Float;
		std::string value_str;
	};
};


class ParseException : public std::runtime_error
{
public:
	ParseException(DictParser& parser, std::string msg) : std::runtime_error(make_error_str(parser,msg)){
	}
private:
	static std::string make_error_str(DictParser& parser, std::string msg) {
		std::string out;
		out += "!!! ";
		out += msg;
		out += "; ";
		out += parser.get_filename();
		out += " on line ";
		out += std::to_string(parser.get_last_line());
		out += "\"";
		out += to_std_string_sv(parser.get_line_str(parser.get_last_line()));
		out += "\"";
		return out;
	}
};

static bool actual_compile_material(const char* name)
{
	std::string path = MATERIAL_DIRECTORY;
	path += name;
	path += ".txt";

	MaterialDef def_file;

	auto file = FileSys::open_read_os(path.c_str());
	if (!file) {
		sys_print("!!! couldn't compile material %s because OS file not found\n", name);
		return false;
	}

	std::vector<uint8_t> buffer;
	buffer.resize(file->size());
	file->read(buffer.data(), buffer.size());
	DictParser parser;
	parser.load_from_memory(buffer.data(), buffer.size(), name);
	StringView tok;

	parser.read_string(tok);
	if (!tok.cmp("TYPE")) {
		throw ParseException(parser, "expected TYPE at start");
	}
	parser.read_string(tok);
	if (!tok.cmp("MaterialMaster")) {
		throw ParseException(parser, "expected MaterialInstance as TYPE");
	}
	while (parser.read_string(tok) && !parser.is_eof())
	{
		if (tok.cmp("_VS_BEGIN")) {
			parser.skip_to_next_line();
			bool found = false;
			while (parser.read_line(tok)) {
				if (tok.cmp("_VS_END")) {
					found = true;
					break;
				}
				def_file.vertex_shader_code += to_std_string_sv(tok);
			}
			if (!found)
				throw ParseException(parser, "expected _VS_END after _VS_BEGIN");
		}
		else if (tok.cmp("_FS_BEGIN")) {
			parser.skip_to_next_line();
			bool found = false;
			while (parser.read_line(tok)) {
				if (tok.cmp("_FS_END")) {
					found = true;
					break;
				}
				def_file.vertex_shader_code += to_std_string_sv(tok);
			}
			if (!found)
				throw ParseException(parser, "expected _VS_END after _VS_BEGIN");
		}
		else if (tok.cmp("PARAMS")) {
			if (!parser.read_string(tok) || !parser.check_item_start(tok))
				throw ParseException(parser, "expected { after PARAMS");
		}
		else if (tok.cmp("PARAM_BUF")) {
			if (!parser.read_string(tok) || !parser.check_item_start(tok))
				throw ParseException(parser, "expected { after PARAM_BUF");



		}
		else if (tok.cmp("CONST_TEX")) {
			if (!parser.read_string(tok) || !parser.check_item_start(tok))
				throw ParseException(parser, "expected { after CONST_TEX");
		}
		else if (tok.cmp("INST_DATA")) {
			if (!parser.read_string(tok) || !parser.check_item_start(tok))
				throw ParseException(parser, "expected { after INST_DATA");
		}
		else if (tok.cmp("SETTINGS")) {
			if (!parser.read_string(tok) || !parser.check_item_start(tok))
				throw ParseException(parser, "expected { after SETTINGS");
		}
		else
			throw ParseException(parser, "unknown key token: " + to_std_string_sv(tok));
	}

	return true;
}

bool MaterialCompilier::compile_new(const char* name)
{
	sys_print("------ Compiling Material %s ------\n", name);
	try {
		bool res = actual_compile_material(name);
		return res;
	}
	catch (ParseException ex) {
		sys_print(ex.what());
		return false;
	}
}