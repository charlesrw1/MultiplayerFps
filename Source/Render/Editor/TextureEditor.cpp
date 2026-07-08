#ifdef EDITOR_BUILD
#include "TextureEditor.h"
#include "Framework/Config.h"

#include <Windows.h>
#include <unordered_set>
#include <optional>
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
void migrate_legacy_tis_compression(TextureImportSettings* tis) {
	if (!tis || tis->compression != TextureCompressionType::Unset)
		return;
	if (tis->load_source_file)
		tis->compression = TextureCompressionType::UseSourceFile;
	else if (tis->is_normalmap)
		tis->compression = TextureCompressionType::NormalMap_BC5;
	else if (tis->make_uncompressed)
		tis->compression = TextureCompressionType::Uncompressed;
	else
		tis->compression = TextureCompressionType::Compressed_BC1;
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

	// Do NOT free hMem after SetClipboardData � clipboard owns it now
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

#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// Opens a native "Open File" dialog filtered to .glb, rooted at the project data dir.
// Returns the picked file as a game-relative path, or nullopt if cancelled or outside the data dir.
std::optional<std::string> OpenGlbFileDialog() {
	char file_buf[MAX_PATH] = {};
	char game_dir_full[MAX_PATH] = {};
	GetFullPathNameA(FileSys::get_game_path(), MAX_PATH, game_dir_full, nullptr);
	std::string game_dir = game_dir_full;

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = "GLB Model (*.glb)\0*.glb\0";
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = game_dir.c_str();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameA(&ofn))
		return std::nullopt;

	std::string picked = file_buf;
	for (auto& c : picked)
		if (c == '\\') c = '/';
	std::string prefix = game_dir + "/";
	if (picked.size() <= prefix.size() ||
		_strnicmp(picked.c_str(), prefix.c_str(), (int)prefix.size()) != 0) {
		sys_print(Error, "OpenGlbFileDialog: picked file is outside the project data dir\n");
		return std::nullopt;
	}
	return picked.substr(prefix.size());
}

// Opens Windows Explorer with the given game-relative asset selected.
void ShowInExplorer(const std::string& game_path) {
	std::string fullpath = FileSys::get_full_path_from_game_path(game_path);
	for (auto& c : fullpath)
		if (c == '/') c = '\\';

	std::string params = "/select,\"" + fullpath + "\"";
	ShellExecuteA(nullptr, "open", "explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

void IMPORT_TEX(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage IMPORT_TEX <.png/.tga/.jpg/.hdr>");
		return;
	}
	std::string gamepath = args.at(1);
	TextureImportSettings tis;
	tis.compression = TextureCompressionType::Compressed_BC1;
	auto findSlash = gamepath.rfind('/');
	tis.src_file = gamepath;
	if (findSlash != std::string::npos)
		tis.src_file = gamepath.substr(findSlash + 1);

	auto path = strip_extension(gamepath) + ".tis";
	write_texture_import_settings(&tis, path);

	Color32 dummy;
	path = strip_extension(gamepath) + ".dds";
	compile_texture_asset(path, dummy);
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
				tis.compression = TextureCompressionType::NormalMap_BC5;
			else
				tis.compression = TextureCompressionType::Compressed_BC1;

			auto path = strip_extension(gamepath) + ".tis";
			write_texture_import_settings(&tis, path);

			Color32 dummy;
			compile_texture_asset(path, dummy);
		}
	}
}

void COMPILE_TEX(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage COMPILE_TEX <.dds>");
		return;
	}
	Color32 dummy;
	compile_texture_asset(args.at(1), dummy);
}
#include "Framework/StringUtils.h"
#define WITH_TEXTURE_COMPILE

std::string turn_gamepath_into_src_path(const std::string& gamepath, const std::string& src_file) {
	auto dir = StringUtils::get_directory(gamepath);
	if (!dir.empty())
		dir += "/";
	dir += src_file;
	return dir;
}

// @docs [[rendering/texture_pipeline#compile_texture_asset flow]]
bool compile_texture_asset(const std::string& gamepath, Color32& outColor) {
#ifdef WITH_TEXTURE_COMPILE
	TextureImportSettings* tis = nullptr;

	{
		// Always compare the compiled .dds output against the .tis settings,
		// regardless of whether gamepath is .png, .dds, or .tis.
		auto texfile = FileSys::open_read_game(strip_extension(gamepath) + ".dds");
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
			ReadSerializerBackendJson reader("compile_texture_asset", to_str, objmaker);
			if (reader.get_root_obj()) {
				tis = reader.get_root_obj()->cast_to<TextureImportSettings>();
				migrate_legacy_tis_compression(tis);
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
				sys_print(Debug, "%s needs compile because .tis is newer than .dds\n", gamepath.c_str());
		} else {
			sys_print(Debug, "%s needs compile because texFile==null\n", gamepath.c_str());
		}

		if (!needsCompile) {
			return true;
		}

		// UseSourceFile means this is a UI/direct-load texture — no texconv needed.
		// The .tis sidecar exists only to carry settings (nearest_filtering, etc.).
		if (tis->compression == TextureCompressionType::UseSourceFile) {
			sys_print(Debug, "%s compression=UseSourceFile (UI texture), skipping compile\n", gamepath.c_str());
			return true;
		}

		const auto dir = turn_gamepath_into_src_path(gamepath, tis->src_file);

		auto src_file = FileSys::open_read(dir.c_str(), FileSys::GAME_DIR);
		if (!src_file) {
			sys_print(Debug, "%s skipping compile because src_file==null\n", gamepath.c_str());
			return false;
		}
	}
	{
		const auto dir = turn_gamepath_into_src_path(gamepath, tis->src_file);

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

	// sRGB only makes sense for color formats (BC1/BC7/uncompressed); normal maps and
	// masks are non-color data and are always sampled linearly.
	using tct = TextureCompressionType;
	string format;
	bool apply_srgb = false;
	switch (tis->compression) {
	case tct::NormalMap_BC5:     format = "BC5_UNORM"; break;
	case tct::GreyscaleMask_BC4: format = "BC4_UNORM"; break;
	case tct::Uncompressed:      format = "R8G8B8A8_UNORM"; break;
	case tct::HighQuality_BC7:   format = tis->is_srgb ? "BC7_UNORM_SRGB" : "BC7_UNORM"; apply_srgb = tis->is_srgb; break;
	case tct::Compressed_BC1:
	default:                     format = tis->is_srgb ? "BC1_UNORM_SRGB" : "BC1_UNORM"; apply_srgb = tis->is_srgb; break;
	}

	std::string commandLine = pathToNvidiaTextureConvertTool + " -f ";
	commandLine += format;

	// otherwise texconv picks up on input .png srgb randomly incorrectly
	if (apply_srgb)
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