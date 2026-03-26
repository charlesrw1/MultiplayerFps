#ifdef EDITOR_BUILD
#include "TextureEditor.h"
#include "Framework/Config.h"

#include <Windows.h>
#include <unordered_set>
// static TextureEditorTool s_texture_editor_tool;
// IEditorTool* g_texture_editor_tool = &s_texture_editor_tool;

#include "Framework/ReflectionProp.h"
#include "Framework/DictWriter.h"
#include "stb_image.h"

#include "Framework/SerializerJson.h"

void write_texture_import_settings(TextureImportSettings* tis, const std::string& path) {
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson writer("write_mis", pathmaker, *tis, true);

	auto fileptr = FileSys::open_write_game(path);
	if (fileptr) {
		sys_print(Debug, "write_texture_import_settings: writing new MIS JSON version %s\n", path.c_str());
		string out = "!json\n" + writer.get_output().dump(1);
		fileptr->write(out.data(), out.size());
	} else {
		sys_print(Error, "write_texture_import_settings Couldnt open file to write out new version of mis %s\n",
				  path.c_str());
	}
}
void SetClipboardText(const std::string& text) {
	if (!OpenClipboard(nullptr))
		return;

	EmptyClipboard();

	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
	if (!hMem) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
	GlobalUnlock(hMem);

	SetClipboardData(CF_TEXT, hMem);

	CloseClipboard();

	// Do NOT free hMem after SetClipboardData — clipboard owns it now
}
#include "Framework/StringUtils.h"
void OpenInNotepad(const string& name) {
	string path = name;
	string stripped = StringUtils::strip_extension(name);
	auto ext = StringUtils::get_extension(name);
	if (ext == ".mi" || ext == ".mm") {

	} else if (ext == ".cmdl") {
		path = stripped + ".mis";
	} else if (ext == ".dds") {
		path = stripped + ".tis";
	} else if (ext == ".tmap") {

	} else {
		sys_print(Warning, "cant open that\n");
		return;
	}

	string fullpath = FileSys::get_full_path_from_game_path(path);

	std::string commandLine = "powershell.exe -Command \"n++ '" + fullpath + "'\"";

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
						&out)) {
		sys_print(Error, "couldn't create process\n");
		return;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
	return;
}

void IMPORT_TEX(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage IMPORT_TEX <.png/.tga/.jpg/.hdr>");
		return;
	}
	std::string gamepath = args.at(1);
	TextureImportSettings tis;
	auto findSlash = gamepath.rfind('/');
	tis.src_file = gamepath;
	if (findSlash != std::string::npos)
		tis.src_file = gamepath.substr(findSlash + 1);

	auto path = strip_extension(gamepath) + ".tis";
	write_texture_import_settings(&tis, path);

	Color32 dummy;
	path = strip_extension(gamepath) + ".dds";
	compile_texture_asset(path, AssetDatabase::loader, dummy);
}
#include "AssetCompile/Someutils.h"
void IMPORT_TEX_FOLDER(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage IMPORT_TEX_FOLDER <folder>");
		return;
	}

	std::unordered_set<std::string> tis_files;
	std::unordered_set<std::string> png_files;
	for (auto file : FileSys::find_game_files_path(args.at(1))) {
		if (get_extension_no_dot(file) == "tis")
			tis_files.insert(file);
		else if (get_extension_no_dot(file) == "png" || get_extension_no_dot(file) == "jpg")
			png_files.insert(file);
	}
	for (auto png_f : png_files) {
		auto tis = strip_extension(png_f) + ".tis";
		if (tis_files.find(tis) == tis_files.end()) {

			auto gamepath = FileSys::get_game_path_from_full_path(png_f);

			TextureImportSettings tis;
			auto findSlash = gamepath.rfind('/');
			tis.src_file = gamepath;
			if (findSlash != std::string::npos)
				tis.src_file = gamepath.substr(findSlash + 1);
			if (gamepath.find("normal") != std::string::npos || gamepath.find("Normal") != std::string::npos ||
				gamepath.find("NRM") != std::string::npos)
				tis.is_normalmap = true;

			auto path = strip_extension(gamepath) + ".tis";
			write_texture_import_settings(&tis, path);

			Color32 dummy;
			compile_texture_asset(path, AssetDatabase::loader, dummy);
		}
	}
}

void COMPILE_TEX(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage COMPILE_TEX <.dds>");
		return;
	}
	Color32 dummy;
	compile_texture_asset(args.at(1), AssetDatabase::loader, dummy);
}
#include "Framework/StringUtils.h"
#define WITH_TEXTURE_COMPILE
bool compile_texture_asset(const std::string& gamepath, IAssetLoadingInterface* loading, Color32& outColor) {
#ifdef WITH_TEXTURE_COMPILE
	TextureImportSettings* tis = nullptr;
	{
		auto texfile = FileSys::open_read_game(gamepath);
		auto tisfile = FileSys::open_read_game(strip_extension(gamepath) + ".tis");
		if (!tisfile) {
			sys_print(Warning, "couldn't find texture import settings file %s\n", gamepath.c_str());
			return false;
		}
		std::string to_str(tisfile->size(), ' ');
		tisfile->read(to_str.data(), tisfile->size());
		uint64_t tisFileTimeStamp = tisfile->get_timestamp();
		tisfile->close();

		if (to_str.find("!json") == 0) {
			to_str = to_str.substr(6);
			MakeObjectFromPathGeneric objmaker;
			ReadSerializerBackendJson reader("compile_texture_asset", to_str, objmaker, *loading);
			if (reader.get_root_obj()) {
				tis = reader.get_root_obj()->cast_to<TextureImportSettings>();
			}
		} else {
			sys_print(Warning, "OLD TIS FORMAT\n");
		}

		if (!tis) {
			sys_print(Error, "couldnt parse texture import settings %s\n", gamepath.c_str());
			return false;
		}

		outColor = tis->simplifiedColor;

		bool needsCompile = texfile == nullptr;
		if (!needsCompile) {
			needsCompile = texfile->get_timestamp() < tisFileTimeStamp;
			if (needsCompile)
				sys_print(Debug, "%s needs compile because texFile is newer tha tisFile\n", gamepath.c_str());
		} else {
			sys_print(Debug, "%s needs compile because texFile==null\n", gamepath.c_str());
		}

		if (!needsCompile) {
			return true;
		}
		auto src_file = FileSys::open_read(tis->src_file.c_str(), FileSys::GAME_DIR);
		if (!src_file) {
			sys_print(Debug, "%s skipping compile because src_file==null\n", gamepath.c_str());
			return false;
		}
	}
	{
		auto dir = StringUtils::get_directory(gamepath);
		if (!dir.empty())
			dir += "/";
		dir += tis->src_file;

		auto imageFile = FileSys::open_read_game(dir);
		if (imageFile) {
			std::vector<char> data;
			data.resize(imageFile->size());
			imageFile->read(data.data(), data.size());
			int outX = 0, outY = 0;
			int channels = 0;
			double sum[4] = {0.0, 0.0, 0.0, 0.0};
			auto outData = stbi_load_from_memory((uint8_t*)data.data(), data.size(), &outX, &outY, &channels,
												 4 /* require 4 channels*/);
			if (outData) {
				for (int y = 0; y < outY; y++) {
					for (int x = 0; x < outX; x++) {
						uint8_t* ptr = &outData[(y * outX + x) * 4];
						sum[0] += ptr[0] / 255.0;
						sum[1] += ptr[1] / 255.0;
						sum[2] += ptr[2] / 255.0;
						sum[3] += ptr[3] / 255.0;
					}
				}
				stbi_image_free(outData);
				int pixelCount = outX * outY;
				for (int i = 0; i < 4; i++)
					sum[i] /= double(pixelCount);

				tis->simplifiedColor.r = sum[0] * 255.0;
				tis->simplifiedColor.g = sum[1] * 255.0;
				tis->simplifiedColor.b = sum[2] * 255.0;
				tis->simplifiedColor.a = sum[3] * 255.0;

				write_texture_import_settings(tis, strip_extension(gamepath) + ".tis");
			} else {
				sys_print(Warning, "compile_texture_asset: stb parse error for source file %s\n",
						  tis->src_file.c_str());
			}
		} else {
			sys_print(Warning, "compile_texture_asset: couldnt open source file %s\n", tis->src_file.c_str());
		}
	}

	std::string parentDir = FileSys::get_full_path_from_game_path(gamepath);
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash + 1);

	const std::string pathToNvidiaTextureConvertTool = "./x64/Debug/texconv.exe";

	string format = "BC1_UNORM";
	if (tis->is_normalmap)
		format = "BC5_UNORM";
	else if (tis->is_srgb)
		format = "BC1_UNORM_SRGB";
	if (tis->make_uncompressed)
		format = "R8_UNORM";

	std::string commandLine = pathToNvidiaTextureConvertTool + " -f ";
	commandLine += format;

	// otherwise texconv picks up on input .png srgb randomly incorrectly
	if (tis->is_srgb)
		commandLine += " -srgb ";

	if (tis->resize_width != 0) {
		commandLine += " -w " + std::to_string(tis->resize_width);
		commandLine += " -h " + std::to_string(tis->resize_width);
	}

	commandLine += " -y ";
	commandLine += " -o " + parentDir;
	commandLine += " " + parentDir + tis->src_file;
	sys_print(Info, "executing texture compile: %s\n", commandLine.c_str());

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};

	delete tis;
	tis = nullptr;

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
						&out)) {
		sys_print(Error, "couldn't create process\n");
		return false;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
#endif
	return true;
}
#endif