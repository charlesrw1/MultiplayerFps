#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "EngineMain.h"
#include "Framework/Util.h"
#include "Scripting/LuaSharpDeclarations.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include "External/json.hpp"
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

bool has_arg(int argc, char** argv, const char* name) {
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == name)
			return true;
	return false;
}

// External game builds deliberately share the same App.exe as engine builds.  When that executable
// is launched directly from <project>/x64/<configuration>/, infer the adjacent game.json/project.ini
// pair and pass the same context that cscli normally supplies.  Engine-owned App.exe lives under
// <engine>/x64/... and has no game.json, so its established startup behavior is untouched.
bool append_external_project_launch_args(int argc, char** argv, std::vector<std::string>& out_args) {
	if (has_arg(argc, argv, "--tests"))
		return false;

	wchar_t exe_buffer[MAX_PATH]{};
	const DWORD length = GetModuleFileNameW(nullptr, exe_buffer, MAX_PATH);
	if (length == 0 || length == MAX_PATH)
		return false;

	std::error_code ec;
	const fs::path executable = fs::weakly_canonical(fs::path(exe_buffer), ec);
	if (ec)
		return false;
	const fs::path configuration_dir = executable.parent_path();
	const fs::path platform_dir = configuration_dir.parent_path();
	if (platform_dir.filename() != "x64")
		return false;
	const fs::path project_root = platform_dir.parent_path();
	const fs::path manifest_path = project_root / "game.json";
	const fs::path project_ini = project_root / "Build" / "project.ini";
	if (!fs::is_regular_file(manifest_path) || !fs::is_regular_file(project_ini))
		return false;

	try {
		std::ifstream manifest_file(manifest_path);
		const nlohmann::json manifest = nlohmann::json::parse(manifest_file);
		fs::path engine_root = manifest.at("engine").get<std::string>();
		if (engine_root.is_relative())
			engine_root = project_root / engine_root;
		engine_root = fs::weakly_canonical(engine_root, ec);
		if (ec || !fs::is_regular_file(engine_root / "CsRemake.sln"))
			return false;

		out_args.assign(argv, argv + argc);
		if (!has_arg(argc, argv, "--engine-root")) {
			out_args.emplace_back("--engine-root");
			out_args.push_back(engine_root.string());
		}
		if (!has_arg(argc, argv, "--project")) {
			out_args.emplace_back("--project");
			out_args.push_back(project_ini.string());
		}
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

// --dump-luasharp-decls <scripts dir>: writes the LuaSharp declarations for the tools and exits.
int dump_luasharp_declarations(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) != "--dump-luasharp-decls")
			continue;
		if (i + 1 >= argc) {
			printf("usage: App.exe --dump-luasharp-decls <scripts dir>\n");
			return 2;
		}
		return write_luasharp_declarations(argv[i + 1]) ? 0 : 1;
	}
	return -1;
}
} // namespace

int main(int argc, char** argv) {
	if (const int dumped = dump_luasharp_declarations(argc, argv); dumped >= 0)
		return dumped;
	MainConfigurationOptions options;
	std::vector<std::string> launch_args;
	if (append_external_project_launch_args(argc, argv, launch_args)) {
		std::vector<char*> launch_argv;
		launch_argv.reserve(launch_args.size());
		for (std::string& arg : launch_args)
			launch_argv.push_back(arg.data());
		return game_engine_main(options, (int)launch_argv.size(), launch_argv.data());
	}
	return game_engine_main(options, argc, argv);
}
