#ifdef EDITOR_BUILD
#include "TextureEditor.h"
#include "Framework/Config.h"

#include <Windows.h>
#include <unordered_set>
//static TextureEditorTool s_texture_editor_tool;
//IEditorTool* g_texture_editor_tool = &s_texture_editor_tool;


#include "Framework/ReflectionProp.h"
#include "Framework/DictWriter.h"
#include "stb_image.h"

#include "Framework/SerializerJson.h"


void write_texture_import_settings(TextureImportSettings* tis, const std::string& path)
{
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson writer("write_mis", pathmaker, *tis, true);

	auto fileptr = FileSys::open_write_game(path);
	if (fileptr) {
		sys_print(Debug, "write_texture_import_settings: writing new MIS JSON version %s\n", path.c_str());
		string out = "!json\n" + writer.get_output().dump(1);
		fileptr->write(out.data(), out.size());
	}
	else {
		sys_print(Error, "write_texture_import_settings Couldnt open file to write out new version of mis %s\n", path.c_str());
	}
}


void IMPORT_TEX(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Error, "usage IMPORT_TEX <.png/.tga/.jpg/.hdr>");
		return;
	}
	std::string gamepath = args.at(1);
	TextureImportSettings tis;
	auto findSlash = gamepath.rfind('/');
	tis.src_file = gamepath;
	if (findSlash != std::string::npos)
		tis.src_file = gamepath.substr(findSlash+1);

	auto path = strip_extension(gamepath) + ".tis";
	write_texture_import_settings(&tis, path);

	Color32 dummy;
	compile_texture_asset(path, AssetDatabase::loader,dummy);
}
#include "AssetCompile/Someutils.h"
void IMPORT_TEX_FOLDER(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Error, "usage IMPORT_TEX_FOLDER <folder>");
		return;
	}

	std::unordered_set<std::string> tis_files;
	std::unordered_set<std::string> png_files;
	for (auto file : FileSys::find_game_files_path(args.at(1))) {
		if (get_extension_no_dot(file) == "tis")
			tis_files.insert(file);
		else if (get_extension_no_dot(file) == "png" || get_extension_no_dot(file)=="jpg")
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
			if (gamepath.find("normal")!=std::string::npos || gamepath.find("Normal") != std::string::npos||gamepath.find("NRM")!=std::string::npos)
				tis.is_normalmap = true;

			auto path = strip_extension(gamepath) + ".tis";
			write_texture_import_settings(&tis, path);

			Color32 dummy;
			compile_texture_asset(path,AssetDatabase::loader,dummy);

		}
	}
}


void COMPILE_TEX(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Error, "usage COMPILE_TEX <.dds>");
		return;
	}
	Color32 dummy;
	compile_texture_asset(args.at(1), AssetDatabase::loader,dummy);
}

#define WITH_TEXTURE_COMPILE
bool compile_texture_asset(const std::string& gamepath, IAssetLoadingInterface* loading, Color32& outColor)
{
#ifdef WITH_TEXTURE_COMPILE
	sys_print(Info,"Compiling texture asset %s\n", gamepath.c_str());
	TextureImportSettings* tis = nullptr;
	{
		auto texfile = FileSys::open_read_game(gamepath);
		auto tisfile = FileSys::open_read_game(strip_extension(gamepath) + ".tis");
		if (!tisfile) {
			sys_print(Warning, "couldn't find texture import settings file\n");
			return false;
		}
		std::string to_str(tisfile->size(), ' ');
		tisfile->read(to_str.data(), tisfile->size());
		uint64_t tisFileTimeStamp = tisfile->get_timestamp();
		tisfile->close();

		if (to_str.find("!json\n") == 0) {
			to_str = to_str.substr(6);
			MakeObjectFromPathGeneric objmaker;
			ReadSerializerBackendJson reader("compile_texture_asset", to_str, objmaker, *loading);
			if (reader.get_root_obj()) {
				tis = reader.get_root_obj()->cast_to<TextureImportSettings>();
			}
		}
		else {
			sys_print(Warning, "OLD TIS FORMAT\n");
		}

		if (!tis) {
			sys_print(Error, "couldnt parse texture import settings\n");
			return false;
		}

		outColor = tis->simplifiedColor;

		bool needsCompile = texfile == nullptr;
		if (!needsCompile) {
			needsCompile = texfile->get_timestamp() < tisFileTimeStamp;
		}
		if (!needsCompile) {
			sys_print(Info,"skipping compile\n");
			return true;
		}
	}

	{
		auto imageFile = FileSys::open_read_game(tis->src_file);
		if (imageFile) {
			std::vector<char> data;
			data.resize(imageFile->size());
			imageFile->read(data.data(), data.size());
			int outX=0, outY=0;
			int channels=0;
			double sum[4] = { 0.0,0.0,0.0,0.0 };
			auto outData = stbi_load_from_memory((uint8_t*)data.data(), data.size(), &outX, &outY, &channels, 4/* require 4 channels*/);
			if (outData) {
				for (int y = 0; y < outY; y++) {
					for (int x = 0; x < outX; x++) {
						uint8_t* ptr = &outData[(y * outX  + x) * 4];
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
				sys_print(Debug, "compile_texture_asset: average: %f %f %f\n", float(sum[0]), float(sum[1]), float(sum[2]));
				
				tis->simplifiedColor.r = sum[0] * 255.0;
				tis->simplifiedColor.g = sum[1] * 255.0;
				tis->simplifiedColor.b = sum[2] * 255.0;
				tis->simplifiedColor.a = sum[3] * 255.0;

				write_texture_import_settings(tis, strip_extension(gamepath) + ".tis");
			}
			else {
				sys_print(Warning, "compile_texture_asset: stb parse error for source file %s\n", tis->src_file.c_str());
			}
		}
		else {
			sys_print(Warning, "compile_texture_asset: couldnt open source file %s\n", tis->src_file.c_str());
		}
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
	sys_print(Info,"executing texture compile: %s\n", commandLine.c_str());

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};
	
	delete tis;
	tis = nullptr;

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &out)) {
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