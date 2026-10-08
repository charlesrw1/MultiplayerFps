#include "ProjectCommands.h"
#include "BridgeClient.h"
#include "Discovery.h"
#include "json.hpp"
#include <Windows.h>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct GameProject
{
	fs::path root;
	fs::path engine_root;
	std::string name;
	std::string application_class;
};

std::string slash_path(const fs::path& path) {
	auto value = fs::weakly_canonical(path).generic_string();
	return value;
}

bool valid_name(const std::string& name) {
	if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_'))
		return false;
	for (char c : name)
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
			return false;
	return true;
}

fs::path engine_root_from_executable() {
	wchar_t buffer[MAX_PATH]{};
	DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (length == 0 || length == MAX_PATH)
		return {};
	return fs::weakly_canonical(fs::path(buffer).parent_path() / ".." / "..");
}

fs::path project_registry_path() {
	wchar_t local_app_data[MAX_PATH]{};
	const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return fs::temp_directory_path() / "CsRemake" / "projects.json";
	return fs::path(local_app_data) / "CsRemake" / "projects.json";
}

json read_project_registry() {
	const fs::path path = project_registry_path();
	std::ifstream file(path);
	if (!file)
		return json::array();
	try {
		json entries = json::parse(file);
		return entries.is_array() ? entries : json::array();
	} catch (...) {
		return json::array();
	}
}

void register_project(const GameProject& project) {
	json entries = read_project_registry();
	const std::string root = slash_path(project.root);
	for (json& entry : entries) {
		if (entry.value("root", std::string()) == root) {
			entry = {{"name", project.name}, {"root", root}, {"engine", slash_path(project.engine_root)}};
			fs::create_directories(project_registry_path().parent_path());
			std::ofstream(project_registry_path()) << entries.dump(2) << "\n";
			return;
		}
	}
	entries.push_back({{"name", project.name}, {"root", root}, {"engine", slash_path(project.engine_root)}});
	fs::create_directories(project_registry_path().parent_path());
	std::ofstream(project_registry_path()) << entries.dump(2) << "\n";
}

fs::path find_manifest(const std::string& requested) {
	fs::path candidate = requested.empty() ? fs::current_path() : fs::path(requested);
	if (candidate.filename() == "game.json")
		return fs::weakly_canonical(candidate);
	if (!requested.empty())
		return fs::weakly_canonical(candidate / "game.json");

	for (;;) {
		fs::path manifest = candidate / "game.json";
		if (fs::exists(manifest))
			return fs::weakly_canonical(manifest);
		fs::path parent = candidate.parent_path();
		if (parent == candidate)
			break;
		candidate = parent;
	}
	return {};
}

bool load_project(const std::string& requested, GameProject& out, std::string& error) {
	fs::path manifest = find_manifest(requested);
	if (manifest.empty() || !fs::exists(manifest)) {
		error = "game.json not found; pass --project or run the command inside a game project";
		return false;
	}

	try {
		std::ifstream file(manifest);
		json data = json::parse(file);
		out.root = manifest.parent_path();
		out.name = data.at("name").get<std::string>();
		out.application_class = data.value("applicationClass", "DefaultGameApplication");
		fs::path engine = data.at("engine").get<std::string>();
		out.engine_root = fs::weakly_canonical(engine.is_absolute() ? engine : out.root / engine);
		if (!valid_name(out.name))
			throw std::runtime_error("name must be a valid C++ identifier");
		if (!fs::exists(out.engine_root / "CsRemake.sln"))
			throw std::runtime_error("engine does not contain CsRemake.sln: " + out.engine_root.string());
	}
	catch (const std::exception& exception) {
		error = std::string("invalid ") + manifest.string() + ": " + exception.what();
		return false;
	}
	return true;
}

void write_text(const fs::path& path, const std::string& text) {
	fs::create_directories(path.parent_path());
	std::ofstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("cannot write " + path.string());
	file << text;
}

std::string xml_escape(const std::string& value) {
	std::string escaped;
	escaped.reserve(value.size());
	for (char c : value) {
		switch (c) {
		case '&': escaped += "&amp;"; break;
		case '"': escaped += "&quot;"; break;
		case '\'': escaped += "&apos;"; break;
		case '<': escaped += "&lt;"; break;
		case '>': escaped += "&gt;"; break;
		default: escaped += c; break;
		}
	}
	return escaped;
}

void write_visual_studio_project(const GameProject& project) {
	const std::string project_file = project.name + ".vcxproj";
	const std::string project_guid = "{BE62F1C0-9F17-4D3B-9BA3-70F06C3EFDD6}";
	const std::string engine = xml_escape(slash_path(project.engine_root));
	const std::string root = xml_escape(slash_path(project.root));
	const std::string name = xml_escape(project.name);

	std::ostringstream vcxproj;
	vcxproj << R"(<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="Current" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <ProjectGuid>)"
			<< project_guid << R"(</ProjectGuid>
    <Keyword>MakeFileProj</Keyword>
    <RootNamespace>)"
			<< name << R"(</RootNamespace>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v145</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v145</PlatformToolset>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup>
    <CsEngineRoot>)"
			<< engine << R"(</CsEngineRoot>
    <CsGameRoot>)"
			<< root << R"(</CsGameRoot>
    <CsGameName>)"
			<< name << R"(</CsGameName>
    <NMakeBuildCommandLine>&quot;$(MSBuildToolsPath)\MSBuild.exe&quot; &quot;$(CsEngineRoot)/CsRemake.sln&quot; /t:App /m /v:minimal /p:Platform=x64 /p:Configuration=$(Configuration) &quot;/p:CsGameRoot=$(CsGameRoot)&quot; /p:CsGameName=$(CsGameName) /p:CsTargetKind=app</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>$(NMakeBuildCommandLine)</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>&quot;$(MSBuildToolsPath)\MSBuild.exe&quot; &quot;$(CsEngineRoot)/CsRemake.sln&quot; /t:App:Clean /m /v:minimal /p:Platform=x64 /p:Configuration=$(Configuration) &quot;/p:CsGameRoot=$(CsGameRoot)&quot; /p:CsGameName=$(CsGameName) /p:CsTargetKind=app</NMakeCleanCommandLine>
    <NMakeOutput>$(CsGameRoot)/x64/$(Configuration)/App.exe</NMakeOutput>
    <NMakeIncludeSearchPath>$(CsGameRoot)/Source;$(CsEngineRoot)/Source</NMakeIncludeSearchPath>
    <LocalDebuggerCommand>$(NMakeOutput)</LocalDebuggerCommand>
    <LocalDebuggerCommandArguments>--editor --engine-root &quot;$(CsEngineRoot)&quot; --project &quot;$(CsGameRoot)/Build/project.ini&quot;</LocalDebuggerCommandArguments>
    <LocalDebuggerWorkingDirectory>$(CsEngineRoot)</LocalDebuggerWorkingDirectory>
  </PropertyGroup>
  <ItemGroup>
    <ClCompile Include="Source\**\*.cpp" />
    <ClInclude Include="Source\**\*.h" />
    <None Include="game.json" /><None Include="Content\**\*" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
)";
	write_text(project.root / project_file, vcxproj.str());

	std::ostringstream solution;
	solution << "Microsoft Visual Studio Solution File, Format Version 12.00\r\n"
			 << "# Visual Studio Version 17\r\n"
			 << "VisualStudioVersion = 17.0.0.0\r\n"
			 << "MinimumVisualStudioVersion = 10.0.40219.1\r\n"
			 << "Project(\"{BC8A1FFA-BEE3-4634-8014-F334798102B3}\") = \"" << project.name << "\", \""
			 << project_file << "\", \"" << project_guid << "\"\r\nEndProject\r\n"
			 << "Global\r\n\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n"
			 << "\t\tDebug|x64 = Debug|x64\r\n\t\tRelease|x64 = Release|x64\r\n\tEndGlobalSection\r\n"
			 << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n"
			 << "\t\t" << project_guid << ".Debug|x64.ActiveCfg = Debug|x64\r\n"
			 << "\t\t" << project_guid << ".Debug|x64.Build.0 = Debug|x64\r\n"
			 << "\t\t" << project_guid << ".Release|x64.ActiveCfg = Release|x64\r\n"
			 << "\t\t" << project_guid << ".Release|x64.Build.0 = Release|x64\r\n"
			 << "\tEndGlobalSection\r\nEndGlobal\r\n";
	write_text(project.root / (project.name + ".sln"), solution.str());
}

fs::path write_runtime_config(const GameProject& project) {
	fs::path path = project.root / "Build" / "project.ini";
	std::ostringstream contents;
	contents << "# Generated by cscli from game.json.\n";
	contents << "g_project_name \"" << project.name << "\"\n";
	contents << "g_project_base \"" << slash_path(project.root / "Content") << "\"\n";
	contents << "g_application_class \"" << project.application_class << "\"\n";
	write_text(path, contents.str());
	return path;
}

fs::path msbuild_path() {
	fs::path preferred = "C:/Program Files/Microsoft Visual Studio/18/Insiders/MSBuild/Current/Bin/amd64/MSBuild.exe";
	return fs::exists(preferred) ? preferred : fs::path("MSBuild.exe");
}

std::wstring widen(const std::string& value) {
	return std::wstring(value.begin(), value.end());
}

int report_error(const std::string& message);

bool is_instance_of_project(const InstanceInfo& instance, const GameProject& project) {
	if (instance.project_name == project.name)
		return true;
	std::error_code ec;
	return !instance.project_file.empty() &&
		fs::equivalent(fs::path(instance.project_file), project.root / "Build" / "project.ini", ec) && !ec;
}

bool stop_running_project_instances(const GameProject& project, std::vector<bool>& relaunch_as_editor) {
	auto instances = discover_instances();
	live_check_instances(instances, 2);
	std::vector<InstanceInfo> matches;
	for (const InstanceInfo& instance : instances) {
		if (instance.alive && is_instance_of_project(instance, project)) {
			matches.push_back(instance);
			relaunch_as_editor.push_back(instance.mode == "editor");
		}
	}

	for (const InstanceInfo& instance : matches)
		bridge_call(instance.port, "run_command", {{"command", "quit"}}, 2);

	// `quit` lets the engine save normal state. It can be blocked by a modal dialog or a broken
	// frame, so this explicitly requested workflow falls back to terminating only the discovered
	// matching process; a normal build never closes anything.
	for (const InstanceInfo& instance : matches) {
		HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(instance.pid));
		if (!process) {
			// The bridge request can complete shutdown before we obtain the process handle.
			// ERROR_INVALID_PARAMETER is Windows' "that PID no longer exists" result.
			if (GetLastError() == ERROR_INVALID_PARAMETER)
				continue;
			return false;
		}
		if (WaitForSingleObject(process, 5000) == WAIT_TIMEOUT) {
			if (!TerminateProcess(process, 1) || WaitForSingleObject(process, 5000) != WAIT_OBJECT_0) {
				CloseHandle(process);
				return false;
			}
		}
		CloseHandle(process);
	}
	return true;
}

int build_target(const GameProject& project, const std::string& configuration, const std::string& target) {
	const std::string kind = target == "test" ? "test" : "app";
	fs::path executable = msbuild_path();
	std::wstring command = L"\"" + executable.wstring() + L"\" \"" + (project.engine_root / "CsRemake.sln").wstring() +
						   L"\" /t:App /m /v:minimal /p:Platform=x64 /p:Configuration=" + widen(configuration) +
						   L" \"/p:CsGameRoot=" + project.root.wstring() + L"\" /p:CsGameName=" + widen(project.name) +
						   L" /p:CsTargetKind=" + widen(kind);
	std::vector<wchar_t> mutable_command(command.begin(), command.end());
	mutable_command.push_back(L'\0');
	STARTUPINFOW startup{sizeof(startup)};
	PROCESS_INFORMATION process{};
	std::wstring cwd = project.engine_root.wstring();
	if (!CreateProcessW(executable.wstring().c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0, nullptr,
						cwd.c_str(), &startup, &process))
		return 1;
	CloseHandle(process.hThread);
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exit_code = 1;
	GetExitCodeProcess(process.hProcess, &exit_code);
	CloseHandle(process.hProcess);
	return static_cast<int>(exit_code);
}

int run_project_tests(const GameProject& project, const std::string& configuration, const std::string& mode,
					  const std::vector<std::string>& patterns) {
	const fs::path executable = project.root / "x64" / configuration / "App.exe";
	if (!fs::exists(executable))
		return report_error("test executable not found: " + executable.string());
	const fs::path runtime_config = write_runtime_config(project);
	std::wstring command = L"\"" + executable.wstring() + L"\" --tests " + widen(mode);
	for (const std::string& pattern : patterns)
		command += L" \"" + widen(pattern) + L"\"";
	command += L" --engine-root \"" + project.engine_root.wstring() + L"\" --project \"" + runtime_config.wstring() + L"\"";
	std::vector<wchar_t> mutable_command(command.begin(), command.end());
	mutable_command.push_back(L'\0');
	STARTUPINFOW startup{sizeof(startup)};
	PROCESS_INFORMATION process{};
	std::wstring cwd = project.engine_root.wstring();
	if (!CreateProcessW(executable.wstring().c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0, nullptr,
						cwd.c_str(), &startup, &process))
		return report_error("failed to run " + executable.string());
	CloseHandle(process.hThread);
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exit_code = 1;
	GetExitCodeProcess(process.hProcess, &exit_code);
	CloseHandle(process.hProcess);
	return static_cast<int>(exit_code);
}

int emit_project(const GlobalOptions& options, const GameProject& project, const char* action) {
	if (options.format == "json") {
		json result = {{"ok", true},
					   {"action", action},
					   {"name", project.name},
					   {"project", slash_path(project.root)},
					   {"engine", slash_path(project.engine_root)}};
		std::cout << result.dump() << "\n";
	} else if (!options.quiet) {
		std::cout << action << " " << project.name << " at " << project.root.string() << "\n";
	}
	return 0;
}

int report_error(const std::string& message) {
	std::cerr << "cscli: " << message << "\n";
	return 1;
}

} // namespace

int cmd_project_new(GlobalOptions& options, const std::string& name, const std::string& requested_path) {
	if (!valid_name(name))
		return report_error("project name must be a valid C++ identifier");

	fs::path root = fs::absolute(requested_path.empty() ? fs::path(name) : fs::path(requested_path));
	if (fs::exists(root) && !fs::is_empty(root))
		return report_error("target directory is not empty: " + root.string());

	GameProject project{fs::weakly_canonical(root), engine_root_from_executable(), name, "DefaultGameApplication"};
	try {
		fs::create_directories(project.root / "Source");
		fs::create_directories(project.root / "Content" / "Scenes");
		fs::create_directories(project.root / "Content" / "scripts");
		fs::create_directories(project.root / "Content" / "scripts" / "tests");
		fs::create_directories(project.root / "Content" / "Models");
		fs::create_directories(project.root / "Content" / "Textures");
		fs::create_directories(project.root / "Tests");
		const fs::path editor_defaults = project.engine_root / "Cfg";
		if (!fs::is_directory(editor_defaults))
			throw std::runtime_error("engine editor configuration defaults not found: " + editor_defaults.string());
		const fs::path starter_assets = project.engine_root / "TestFilesData" / "eng";
		if (!fs::is_directory(starter_assets))
			throw std::runtime_error("engine starter assets not found: " + starter_assets.string());

		json manifest = {{"name", name}, {"engine", slash_path(project.engine_root)}};
		write_text(project.root / "game.json", manifest.dump(2) + "\n");
		write_text(project.root / ".gitignore", "Build/\nx64/\nDist/\nCache/\nLogs/\nConfig/Editor/\n");
		// External projects keep their editor layout separate from the engine checkout.
		// Seed it with every supplied ImGui layout so first launch has the same defaults.
		fs::create_directories(project.root / "Config");
		fs::copy(editor_defaults, project.root / "Config" / "Editor", fs::copy_options::recursive);
		write_text(project.root / "Source" / (name + "Component.h"),
				   "#pragma once\n#include \"Game/EntityComponent.h\"\n\nclass " + name +
				   "Component : public Component\n{\npublic:\n\tCLASS_BODY(" + name +
					   "Component, spawnable);\n\n\tREFLECT()\n\tfloat speed = 1.0f;\n\tREF int starting_lives = 3;\n};\n");
		write_text(project.root / "Source" / (name + "Component.cpp"), "#include \"" + name + "Component.h\"\n");
		fs::copy(starter_assets, project.root / "Content" / "eng", fs::copy_options::recursive);
		// This bundle indexes the engine's entire sample asset tree. A newly created project
		// only receives the small starter subset, so loading it would produce missing-asset
		// errors for unrelated sample content.
		fs::remove(project.root / "Content" / "eng" / "default_bundle.bundle");
		const fs::path engine_assets = project.engine_root / "TestFilesData";
		for (const char* name : {"defaultPBR.mm", "defaultPBR_shader.glsl", "defaultPBRmasked.mm",
				 "defaultPBRmasked_shader.glsl", "transparent_debug.mm", "transparent_debug_shader.glsl",
				 "transparent_debug.dds", "transparent_debug.png", "transparent_debug.tis", "particle_default.mm",
				 "particle_default_shader.glsl", "bricksDefault.mm", "bricksDefault_shader.glsl", "woodGrunge.mi",
				 "wornpaintedwoodsiding-albedo2.dds", "wornpaintedwoodsiding-normal-ogl.dds",
				 "wornpaintedwoodsiding-roughness.dds"})
			fs::copy_file(engine_assets / name, project.root / "Content" / name);
		for (const char* name : {"fallback.mm", "fallback_shader.glsl"})
			fs::copy_file(starter_assets / name, project.root / "Content" / name);
		fs::copy(engine_assets / "ui", project.root / "Content" / "ui", fs::copy_options::recursive);
		write_text(project.root / "Content" / "Material.mi",
				   "TYPE MaterialInstance\nPARENT defaultPBR.mm\nVAR Albedo _white\nVAR Normalmap _flat_normal\n"
				   "VAR Roughness _white\nVAR AOMap _white\nVAR MetalMap _white\n");
		for (const char* name : {"sphere.mis", "sphere.cmdl", "sphere.glb", "cube1m.mis", "cube1m.cmdl", "cube1m.glb"})
			fs::copy_file(engine_assets / name, project.root / "Content" / name);
		fs::copy_file(starter_assets / "template.tmap", project.root / "Content" / "Scenes" / "Main.tmap");
		write_text(project.root / "Content" / "scripts" / "main.lua", "-- " + name + " gameplay scripts\n");
		fs::copy_file(project.engine_root / "TestFilesData" / "scripts" / "integration_test_framework.lua",
				  project.root / "Content" / "scripts" / "integration_test_framework.lua");
		write_runtime_config(project);
		write_visual_studio_project(project);
	}
	catch (const std::exception& exception) {
		return report_error(exception.what());
	}
	register_project(project);
	return emit_project(options, project, "created");
}

int cmd_project_build(GlobalOptions& options, const std::string& requested, const std::string& configuration,
					  const std::string& target, bool restart_running) {
	GameProject project;
	std::string error;
	if (!load_project(requested, project, error))
		return report_error(error);
	register_project(project);
	write_runtime_config(project);
	std::vector<bool> relaunch_as_editor;
	if (restart_running && !stop_running_project_instances(project, relaunch_as_editor))
		return report_error("could not stop every running editor/game instance for this project");

	if (target != "editor" && target != "game" && target != "test" && target != "all")
		return report_error("--target must be editor, game, test, or all");
	if (target == "editor" || target == "game" || target == "all")
		if (int result = build_target(project, configuration, "app"); result != 0)
			return result;
	if (target == "test")
		if (int result = build_target(project, configuration, "test"); result != 0)
			return result;
	for (bool editor : relaunch_as_editor) {
		int result = cmd_project_launch(options, project.root.string(), configuration, editor, false, true);
		if (result != 0)
			return result;
	}
	return emit_project(options, project, "built");
}

int cmd_project_launch(GlobalOptions& options, const std::string& requested, const std::string& configuration,
					   bool editor, bool build_first, bool wait_ready) {
	GameProject project;
	std::string error;
	if (!load_project(requested, project, error))
		return report_error(error);
	if (build_first) {
		int result = cmd_project_build(options, requested, configuration, editor ? "editor" : "game");
		if (result != 0)
			return result;
	}

	fs::path runtime_config = write_runtime_config(project);
	fs::path bin = project.root / "x64" / configuration;
	fs::path executable = bin / "App.exe";
	if (!fs::exists(executable))
		return report_error("executable not found: " + executable.string());

	std::wstring command = L"\"" + executable.wstring() + L"\"";
	if (editor)
		command += L" --editor";
	command +=
		L" --engine-root \"" + project.engine_root.wstring() + L"\" --project \"" + runtime_config.wstring() + L"\"";
	std::vector<wchar_t> mutable_command(command.begin(), command.end());
	mutable_command.push_back(L'\0');
	STARTUPINFOW startup{sizeof(startup)};
	PROCESS_INFORMATION process{};
	std::wstring cwd = project.engine_root.wstring();
	if (!CreateProcessW(executable.wstring().c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0, nullptr,
						cwd.c_str(), &startup, &process))
		return report_error("failed to launch " + executable.string());
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	emit_project(options, project, editor ? "launched editor" : "launched game");

	if (wait_ready) {
		GlobalOptions wait_options = options;
		wait_options.project_filter = project.name;
		return cmd_wait_ready(wait_options, 60);
	}
	return 0;
}

int cmd_project_test(GlobalOptions& options, const std::string& requested, const std::string& configuration,
					 bool build_first, const std::string& mode, const std::vector<std::string>& patterns) {
	if (mode != "editor" && mode != "game")
		return report_error("--mode must be editor or game");
	GameProject project;
	std::string error;
	if (!load_project(requested, project, error))
		return report_error(error);
	if (build_first) {
		int result = cmd_project_build(options, requested, configuration, "test");
		if (result != 0)
			return result;
	}
	int result = run_project_tests(project, configuration, mode, patterns);
	if (result == 0)
		emit_project(options, project, "tested");
	return result;
}

int cmd_projects(GlobalOptions& options) {
	json entries = read_project_registry();
	if (options.format == "json") {
		std::cout << entries.dump() << "\n";
		return 0;
	}
	for (const json& entry : entries)
		std::cout << entry.value("name", std::string("<unnamed>")) << "\t"
				  << entry.value("root", std::string()) << "\n";
	return 0;
}

int cmd_shader_cache_clear(GlobalOptions& options, const std::string& requested, bool all_projects) {
	std::vector<fs::path> roots;
	if (all_projects) {
		for (const json& entry : read_project_registry())
			if (entry.contains("root") && entry["root"].is_string())
				roots.emplace_back(entry["root"].get<std::string>());
	} else {
		GameProject project;
		std::string error;
		if (!load_project(requested, project, error))
			return report_error(error);
		roots.push_back(project.root);
	}

	for (const fs::path& root : roots) {
		const fs::path cache = root / "Cache" / "ShaderCache";
		std::error_code ec;
		const uintmax_t removed = fs::remove_all(cache, ec);
		if (ec)
			return report_error("could not clear " + cache.string() + ": " + ec.message());
		if (!options.quiet)
			std::cout << "cleared " << cache.string() << " (" << removed << " entries)\n";
	}
	return 0;
}
