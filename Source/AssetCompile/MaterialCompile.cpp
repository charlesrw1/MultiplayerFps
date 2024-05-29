#include "Compiliers.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Someutils.h"
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