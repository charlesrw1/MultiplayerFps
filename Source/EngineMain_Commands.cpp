#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL2/SDL.h>
#include "glad/glad.h"
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "GameEngineLocal.h"
#include "Level.h"
#include "IEditorTool.h"
#include "User_Camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Framework/ClassBase.h"
#include "Framework/MeshBuilder.h"
#include "Framework/Files.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Game/Entities/Player.h"
#include "Game/Entity.h"
#include "Game/LevelAssets.h"
#include "Game/Components/CameraComponent.h"
#include "Physics/Physics2.h"
#include "Assets/AssetBrowser.h"
#include "Sound/SoundPublic.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "Framework/EditorTheme.h"
#include "UI/UILoader.h"
#include "UI/Widgets/Layouts.h"
#include "UI/OnScreenLogGui.h"
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Input/InputSystem.h"
#include "Render/RenderObj.h"
#include "Render/ModelManager.h"
#include "Framework/SysPrint.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"
#include "Framework/Jobs.h"
#include "EditorPopups.h"
#ifdef EDITOR_BUILD
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/EditorRecents.h"
#endif
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/StringUtils.h"
#include "EngineMain.h"
#include "EditorPopupTemplate.h"
#include "Animation/SkeletonData.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/TestRunner.h"
#include "IntegrationTests/StateDump.h"
#include "Framework/Util.h"
#include <mutex>

#include "Logging.h"

extern GameEngineLocal eng_local;
extern ConfigVar g_editor_cfg_folder;

vector<string>* GameEngineLocal::find_keybinds(SDL_Scancode code, uint16_t keymod) {

	auto mod_to_integer = [](uint16_t mod) -> uint16_t {
		if (mod & KMOD_CTRL)
			mod |= KMOD_CTRL;
		if (mod & KMOD_SHIFT)
			mod |= KMOD_SHIFT;
		if (mod & KMOD_ALT)
			mod |= KMOD_ALT;
		mod &= KMOD_CTRL | KMOD_SHIFT | KMOD_ALT;
		return mod;
	};

	uint32_t both = uint32_t(code) | ((uint32_t)mod_to_integer(keymod) << 16);

	auto find = keybinds.find(both);
	if (find == keybinds.end())
		return nullptr;
	return &find->second;
}

void GameEngineLocal::set_keybind(SDL_Scancode code, uint16_t keymod, std::string bind) {
	auto mod_to_integer = [](uint16_t mod) -> uint16_t {
		if (mod & KMOD_CTRL)
			mod |= KMOD_CTRL;
		if (mod & KMOD_SHIFT)
			mod |= KMOD_SHIFT;
		if (mod & KMOD_ALT)
			mod |= KMOD_ALT;
		mod &= KMOD_CTRL | KMOD_SHIFT | KMOD_ALT;
		return mod;
	};
	uint32_t both = uint32_t(code) | ((uint32_t)mod_to_integer(keymod) << 16);
	keybinds[both].push_back(bind);
}

void bind_key(const Cmd_Args& args) {
	if (args.size() < 2)
		return;
	SDL_Scancode scancode = SDL_GetScancodeFromName(args.at(1));
	if (scancode == SDL_SCANCODE_UNKNOWN)
		return;
	if (args.size() <= 2)
		eng_local.set_keybind(scancode, 0, "");
	else if (args.size() <= 3)
		eng_local.set_keybind(scancode, 0, args.at(2));
	else {

		uint16_t modifiers = 0;
		for (int i = 2; i < args.size() - 1; i++) {
			const char* m = args.at(i);
			if (strcmp(m, "Ctrl") == 0)
				modifiers |= KMOD_CTRL;
			else if (strcmp(m, "Alt") == 0)
				modifiers |= KMOD_ALT;
			else if (strcmp(m, "Shift") == 0)
				modifiers |= KMOD_SHIFT;
			else
				sys_print(Warning, "unknown modifier for 'bind': %s\n", m);
		}

		// bind M Ctrl Alt "mycommand"

		eng_local.set_keybind(scancode, modifiers, args.at(args.size() - 1));
	}
}

static void inc_or_dec_int_var(ConfigVar* var, bool decrement) {
	int cur = var->get_integer();
	int max = std::round(var->get_max_val());
	int min = std::round(var->get_min_val());
	int span = max - min;
	int cur_start_min = cur - min;
	int next_start_min = (cur_start_min + 1) % span;
	if (decrement) {
		next_start_min = cur_start_min - 1;
		if (next_start_min < 0)
			next_start_min = span - 1;
	}
	int next = next_start_min + min;
	var->set_integer(next);
	sys_print(Info, "%s = %s\n", var->get_name(), var->get_string());
}

void dump_imgui_ini(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Info, "usage: dump_imgui_ini  ($g_editor_cfg_folder)/<file>");
		return;
	}

	std::string relative = g_editor_cfg_folder.get_string();
	relative += "/";
	relative += args.at(1);

	auto path = FileSys::get_full_path_from_relative(relative, FileSys::ENGINE_DIR); // might change this to user dir

	ImGui::SaveIniSettingsToDisk(path.c_str());
}
void load_imgui_ini(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Info, "usage: load_imgui_ini ($g_editor_cfg_folder)/<file>");
		return;
	}

	std::string relative = g_editor_cfg_folder.get_string();
	relative += "/";
	relative += args.at(1);

	auto path = FileSys::get_full_path_from_relative(relative, FileSys::ENGINE_DIR);

	ImGui::LoadIniSettingsFromDisk(path.c_str());
}

// tech debt nonsense
extern void IMPORT_TEX_FOLDER(const Cmd_Args& args);
extern void IMPORT_TEX(const Cmd_Args& args);
extern void COMPILE_TEX(const Cmd_Args& args);
extern void dump_render_memory_usage();
void GameEngineLocal::add_commands() {
	commands = ConsoleCmdGroup::create("");
#ifdef EDITOR_BUILD
	g_editor_recents.load();
#endif
	commands->add("print_assets", [](const Cmd_Args&) { g_assets.print_usage(); });
#ifdef EDITOR_BUILD
	commands->add("import-tex-folder", IMPORT_TEX_FOLDER);
	commands->add("import-tex", IMPORT_TEX);
	commands->add("compile-tex", COMPILE_TEX);

#endif
	commands->add("dump_render_memory_usage", [](const Cmd_Args&) { dump_render_memory_usage(); });
	static int blah = 0;
	commands->add("stress-test", [](const Cmd_Args&) {
		int size = 20;
		for (int x = 0; x < size; x++) {
			for (int y = 0; y < 10; y++) {
				for (int z = 0; z < size; z++) {
					Entity* e = eng->get_level()->spawn_entity();
					auto m = e->create_component<MeshComponent>();
					e->dont_serialize_or_edit = true;
					m->dont_serialize_or_edit = true;
					m->set_ignore_baking(true);
					m->set_model(Model::load("work_prop/gas_cylinder.cmdl"));
					e->set_ws_position(glm::vec3(x, y + blah * 10, z) * 4.0f);
				}
			}
		}
		blah += 1;
	});
	commands->add("save_baked_gi", [](const Cmd_Args&) { GameSceneGiUtil::save_to_disk(); });
	commands->add("bake_probes", [](const Cmd_Args&) { GameSceneGiUtil::bake_all_cubemaps(); });
	// commands->add("close_ed", close_editor);
	commands->add("load_imgui_ini", load_imgui_ini);
	commands->add("dump_imgui_ini", dump_imgui_ini);
	commands->add("reload_shaders", [](const Cmd_Args&) { idraw->reload_shaders(); });
	commands->add("dec", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: dec <int cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_INTEGER)) {
			sys_print(Warning, "usage: dec <int cvar>");
			return;
		}
		inc_or_dec_int_var(var, true);
	});
	commands->add("inc", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: inc <int cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_INTEGER)) {
			sys_print(Warning, "usage: inc <int cvar>");
			return;
		}
		inc_or_dec_int_var(var, false);
	});
	commands->add("toggle", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: toggle <boolean cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_BOOL)) {
			sys_print(Warning, "usage: toggle <boolean cvar>");
			return;
		}
		var->set_bool(!var->get_bool());
		sys_print(Info, "%s = %s\n", var->get_name(), var->get_string());
	});
	commands->add("exec", [](const Cmd_Args& args) {
		if (args.size() < 2) {
			sys_print(Info, "usage: exec <exec filename>");
			return;
		}
		Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, args.at(1));
	});
	commands->add("quit", [](const Cmd_Args& args) { Quit(); });

	commands->add("create-map", [](const Cmd_Args& args) {
		auto existing = FileSys::open_read_game(args.at(1));
		if (!existing) {
			auto file = FileSys::open_write_game(args.at(1));
			string s = "!json\n{\"objs\":[]}";
			file->write(s.c_str(), s.size());
		} else {
			sys_print(Error, "cant make new map, already exists\n");
		}
	});
	commands->add("open-editor", [&](const Cmd_Args& args) {
		sys_print(Debug, "OpenEditorToolCommand::execute\n");

		if (!eng_local.is_editor_state()) {
			sys_print(
				Error,
				"OpenEditorToolCommand: didnt launch in editor mode, pass --editor on the command line\n");
			return;
		}
		string mapname = "<empty>";
		if (args.size() == 2)
			mapname = args.at(1);

		open_tool(mapname);
	});

#ifdef EDITOR_BUILD
	commands->add("recent", [&](const Cmd_Args& args) {
		if (!eng_local.is_editor_state()) {
			args.sys_print(Error, "recent: editor not active (pass --editor)\n");
			return;
		}
		if (args.size() == 1) {
			g_editor_recents.print_list(args);
			return;
		}
		if (args.size() != 2) {
			args.sys_print(Warning, "usage: recent | recent <slot>\n");
			return;
		}
		const int slot = std::atoi(args.at(1));
		auto entry = g_editor_recents.at_slot(slot);
		if (!entry) {
			args.sys_print(Warning, "recent: invalid slot %d (have %d entries)\n", slot, g_editor_recents.size());
			return;
		}
		// Copy by value: open_tool may push to the deque, invalidating element refs.
		const string path = entry->path;
		const CameraSnapshot cam = entry->camera;
		open_tool(path);
		if (auto* doc = dynamic_cast<EditorDoc*>(this->editor_tool.get())) {
			if (doc->get_asset_path() == path)
				doc->ed_cam.apply_snapshot(cam);
		}
	});
#endif

	commands->add("bind", bind_key);

	commands->add("dump_bundle", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: bundle name\n");
		} else {
			g_assets.dump_loaded_assets_to_disk(args.at(1));
		}
	});
	commands->add("reload_script", [](const Cmd_Args& args) { ScriptManager::inst->reload_all_scripts(); });

	g_modelMgr.add_commands(*commands);
}

#include "LevelSerialization/SerializeNew.h"
bool GameEngineLocal::load_level(string mapname) {
	if (level && level->get_is_in_update()) {
		sys_print(Warning, "GameEngineLocal::load_level: level is in update period, can't change level here.\n");
		return false;
	}
	const bool wants_empty = mapname == "<empty>" || mapname.empty();

	double start_time = GetTime();

	bool success = true;
	uptr<UnserializedSceneFile> file;
	if (!wants_empty) {
		auto val = load_level_asset(mapname);
		if (val)
			file = std::move(val);
		else
			success = false;
	} else {
		file = std::make_unique<UnserializedSceneFile>();
	}

	auto insert_this_map_as_level = [&](UnserializedSceneFile* loadedLevel, bool is_for_playing) {
		sys_print(Info, "Changing map: %s (for_playing=%s)\n", mapname.c_str(), print_get_bool_string(is_for_playing));

#ifdef EDITOR_BUILD
		if (editor_tool) {
			editor_tool.reset();
			//assert(!editorState->get_tool());
		}
#endif
		if (level) {
			stop_game();
			assert(!level);
		}

		g_modelMgr.compact_memory(); // fixme, compacting memory here means newly loaded objs get moved twice, should be
									 // queuing uploads
		time = 0.0;
		set_tick_rate(60.f);
		level = std::make_unique<Level>(!is_for_playing);
		level->start(mapname, loadedLevel); // scene will then get destroyed
		idraw->on_level_start();

		if (app) {
			app->on_map_changed();
		}
		sys_print(Info, "changed state to Engine_State::Game\n");
	};

	if (success || wants_empty) {
		insert_this_map_as_level(file.get(), !is_editor_state());
	} else {
		sys_print(Warning, "OpenMapCommand::execute(%s): failed to load\n", mapname.c_str());
		return false;
	}

	double now = GetTime();
	sys_print(Debug, "OpenMapCommand::execute: took %f\n", float(now - start_time));

	return true;
}
