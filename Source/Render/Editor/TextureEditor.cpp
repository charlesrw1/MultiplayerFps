#include "TextureEditor.h"
#include "Framework/Config.h"

#include <Windows.h>

static TextureEditorTool s_texture_editor_tool;
IEditorTool* g_texture_editor_tool = &s_texture_editor_tool;

CLASS_IMPL(TextureImportSettings);




DECLARE_ENGINE_CMD(IMPORT_TEX)
{
	if (args.size() != 2) {
		sys_print("!!! usage IMPORT_TEX <.png/.tga/.jpg/.hdr>");
		return;
	}
	std::string gamepath = args.at(1);
	TextureImportSettings tis;
	auto findSlash = gamepath.rfind('/');
	tis.src_file = gamepath;
	if (findSlash != std::string::npos)
		tis.src_file = gamepath.substr(findSlash+1);
	
	DictWriter out;
	write_object_properties(&tis, nullptr, out);
	auto outfile = FileSys::open_write_game(strip_extension(gamepath) + ".tis");
	assert(outfile);
	outfile->write(out.get_output().data(), out.get_output().size());
	outfile->close();

	compile_texture_asset(strip_extension(gamepath) + ".dds");
}

DECLARE_ENGINE_CMD(COMPILE_TEX)
{
	if (args.size() != 2) {
		sys_print("!!! usage COMPILE_TEX <.dds>");
		return;
	}
	compile_texture_asset(args.at(1));
}

#define WITH_TEXTURE_COMPILE
bool compile_texture_asset(const std::string& gamepath)
{
#ifdef WITH_TEXTURE_COMPILE
	sys_print("*** Compiling texture asset %s\n", gamepath.c_str());
	auto texfile = FileSys::open_read_game(gamepath);
	auto tisfile = FileSys::open_read_game(strip_extension(gamepath) + ".tis");
	if (!tisfile) {
		sys_print("!!! couldn't find texture import settings file\n");
		return false;
	}
	bool needsCompile = texfile == nullptr;
	if (!needsCompile) {
		needsCompile = texfile->get_timestamp() < tisfile->get_timestamp();
	}
	if (!needsCompile) {
		sys_print("*** skipping compile\n");
		return true;
	}

	DictParser in;
	in.load_from_file(tisfile.get());
	auto tis = read_object_properties_no_input_tok<TextureImportSettings>(nullptr, in);
	if (!tis) {
		sys_print("!!! couldnt parse texture import settings\n");
		return false;
	}

	std::string parentDir = FileSys::get_full_path_from_game_path(gamepath);
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash+1);

	const std::string pathToNvidiaTextureConvertTool = "./x64/Debug/texconv.exe";

	const std::string format = (tis->is_normalmap) ? "BC5_UNORM" : "BC1_UNORM";
	std::string commandLine = pathToNvidiaTextureConvertTool + " -f ";
	commandLine += format;
	commandLine += " -y ";
	commandLine += " -o " + parentDir;
	commandLine += " " + parentDir + tis->src_file;
	sys_print("*** executing texture compile: %s\n", commandLine.c_str());

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};
	
	delete tis;
	tis = nullptr;

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &out)) {
		sys_print("!!! couldn't create process\n");
		return false;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
#endif
	return true;
}