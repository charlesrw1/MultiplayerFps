#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL2/SDL.h>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "GameEngineLocal.h"
#include "Level.h"
#include "IEditorTool.h"
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
#include "Game/Entity.h"
#include "Game/LevelAssets.h"
#include "Game/Components/CameraComponent.h"
#include "Physics/Physics2.h"
#include "Sound/SoundPublic.h"
#include "imgui.h"
#include "Render/IGraphicsDevice.h"
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
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Framework/StringUtils.h"
#include "EngineMain.h"
#include "IntegrationTests/TestRunner.h"
#include "Framework/Util.h"
#include <mutex>

#include "Logging.h"

extern GameEngineLocal eng_local;
extern ConfigVar g_window_fullscreen;
extern ConfigVar g_engine_ticks_per_frame;
extern ConfigVar g_slomo;

// Free functions defined in EngineMain_Debug.cpp
extern void debug_shape_ctx_update(float dt);
extern void debug_shape_ctx_fixed_update_start();

struct GameUpdateOuput
{
	bool drawOut = false;
	SceneDrawParamsEx paramsOut = SceneDrawParamsEx(0, 0);
	View_Setup vsOut;
};

static void lua_error_loop(string msg, auto&& frame_start, auto&& wait_for_swap, SceneDrawParamsEx drawparamsNext,
						   View_Setup setupNext) {
	// bsod is funny lol
	sys_print(Error, "loop: caught LuaRuntimeError: %s\n", msg.c_str());
	auto lines = StringUtils::to_lines(msg);
	//	drawparamsNext.draw_world = false;
	drawparamsNext.draw_ui = true;

	while (1) {
		SDL_Delay(10);

		gfx().begin_frame();
		frame_start();

		if (Input::was_key_pressed(SDL_SCANCODE_RETURN))
			break;

		RectangleShape shape;
		shape.color = {50, 50, 255, 180};
		shape.rect.w = setupNext.width;
		shape.rect.h = setupNext.height;
		UiSystem::inst->window.draw(shape);
		TextShape text;
		text.font = GuiFont::load("eng/fonts/monospace12.fnt");
		text.color = COLOR_WHITE;
		text.rect.x = 5;
		text.rect.y = 5;
		for (auto& l : lines) {
			text.text = l;
			text.rect.y += text.font->lineHeight;
			UiSystem::inst->window.draw(text);
		}
		text.rect.y += text.font->lineHeight * 5;
		text.text = "[PRESS ENTER TO CONTINUE]";
		UiSystem::inst->window.draw(text);

		UiSystem::inst->sync_to_renderer();
		idraw->sync_update();
		idraw->scene_draw(drawparamsNext, setupNext);
		wait_for_swap(false);
	}
	ScriptManager::inst->check_for_reload();
}

void GameEngineLocal::game_update_tick() {
	ZoneScopedN("game_update_tick");

	auto fixed_update = [&](double dt) {
		ZoneScopedN("fixed_update");

		debug_shape_ctx_fixed_update_start();
		g_physics.simulate_and_fetch(dt);
	};
	auto update = [&](double dt) {
		ZoneScopedN("update");
		CPUSCOPESTART(game_update_tick_update);
		if (level)
			level->update_level();
		if (app)
			app->update();
		GameAnimationMgr::inst->update_animating();
	};
	const double dt = frame_time;

	double secs_per_tick = tick_interval;
	frame_remainder += dt;
	int num_ticks = (int)floor(frame_remainder / secs_per_tick);
	frame_remainder -= num_ticks * secs_per_tick;

	float orig_ft = frame_time;
	float orig_ti = tick_interval;

	frame_time *= g_slomo.get_float();
	tick_interval *= g_slomo.get_float();

	// physics update

	if (app)
		app->pre_update();

	for (int i = 0; i < 1; i++) {
		fixed_update(tick_interval);
	}

	// call level update
	update(frame_time);

	time += frame_time;

	frame_time = orig_ft;
	tick_interval = orig_ti;
}

// unloads all game state
void GameEngineLocal::stop_game() {
	if (!map_spawned())
		return;

	assert(level);
	string name = level->get_source_asset_name();
	sys_print(Info, "Clearing Map (%s)\n", name.c_str());
	on_leave_level.invoke();
	idraw->on_level_end();
	level->close_level();
	level.reset();

	// clear any debug shapes
	debug_shape_ctx_fixed_update_start();
}

bool GameEngineLocal::game_thread_update() {

	return true;
}

void GameEngineLocal::loop() {
	auto frame_start = [&]() {
		ZoneScopedN("frame_start");
		// update input
		Input::inst->pre_events();
		UiSystem::inst->pre_events();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			gfx().imgui_process_event(&event);
			Input::inst->handle_event(event);
			UiSystem::inst->handle_event(event);

			switch (event.type) {
			case SDL_QUIT:
				::Quit();
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int x, y;
					SDL_GetWindowSize(window, &x, &y);
					if (x % 2 == 1)
						x -= 1;
					if (y % 2 == 1)
						y -= 1;
					bool changed = false;
					if (x != g_window_w.get_integer()) {
						changed = true;
						g_window_w.set_integer(x);
					}
					if (y != g_window_h.get_integer()) {
						changed = true;
						g_window_h.set_integer(y);
					}
					if (changed)
						SDL_SetWindowSize(window, x, y);
				}
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
				key_event(event);
				break;
			default:
				break;
			}
			// if (!is_game_focused()) {
			//	if ((event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) && ImGui::GetIO().WantCaptureKeyboard)
			//		continue;
			//	if (!scene_hovered && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) &&
			// ImGui::GetIO().WantCaptureMouse) 		continue;
			//}
			// g_guiSystem->handle_event(event);
		}
		Input::inst->tick();
		UiSystem::inst->update();
		// Update the messsage queue! does level changing etc.
		Cmd_Manager::inst->execute_buffer();

		if (g_window_fullscreen.was_changed())
			Canvas::set_window_fullscreen(g_window_fullscreen.get_bool());

		if (g_engine_ticks_per_frame.get_integer() <= 1) {
			sub_tick_index_ = -1;
		}
		else {
			if (sub_tick_index_ <= 0)
				sub_tick_index_ = g_engine_ticks_per_frame.get_integer();
			sub_tick_index_ -= 1;

			frame_time = tick_interval;
		}
	};

	auto do_overlapped_update = [&](bool& shouldDrawNext, SceneDrawParamsEx& drawparamsNext, View_Setup& setupNext,
									const bool skip_rendering) {
		ZoneScopedN("OverlappedUpdate");
		CPUSCOPESTART(OverlappedUpdate);
		BooleanScope scope(b_is_in_overlapped_period);
		GameUpdateOuput out;

		// I reworked the asset system so have to disable this for now. issue is sync loading assets on game thread.
		// otherwise everything else is threadsafe(tm).
		//
		// JobCounter* gameupdatecounter{};
		// JobSystem::inst->add_job(game_update_job,uintptr_t(&out), gameupdatecounter);
		// game_update_job(uintptr_t(&out));

		{
			ZoneScopedN("GameThreadUpdate");
			CPUFUNCTIONSTART;

			// printf("abc\n");
			out.drawOut = true;
			game_update_tick();
#ifdef EDITOR_BUILD
			if (editor_tool)
				editor_tool->tick(frame_time);
#endif

			isound->tick(frame_time);

			eng_local.get_draw_params(out.paramsOut, out.vsOut);
			// return;
			// update particles, doesnt draw, only builds meshes FIXME
			ParticleMgr::inst->draw(out.vsOut);
		}

		if (!skip_rendering) {
			if (!shouldDrawNext) {
				drawparamsNext.draw_world = drawparamsNext.draw_ui = false;
			}
			idraw->scene_draw(drawparamsNext, setupNext);
		}

		shouldDrawNext = out.drawOut;
		drawparamsNext = out.paramsOut;
		setupNext = out.vsOut;
		// JobSystem::inst->wait_and_free_counter(gameupdatecounter);// wait for game update to finish while render is
		// on this thread
	};

	// This happens on main thread
	// I could double buffer draw data so ImGui can update on game thread and render simultaneously
	auto imgui_render = [&](const bool skip_rendering) {
		// ZoneScopedN("ImguiDraw");
		GPUSCOPESTART(imgui_update_scope);

		ZoneScopedN("ImGuiUpdate");

		gui_log.draw(UiSystem::inst->window);

		UiSystem::inst->draw_imgui_interfaces(editor_tool.get());

		ImGui::Render();
		if (!skip_rendering) {
			gfx().imgui_render_draw_data();
		}
	};
	auto do_sync_update = [&]() {
		ZoneScopedN("SyncUpdate");
		debug_shape_ctx_update(frame_time);
#ifdef EDITOR_BUILD
		AssetRegistrySystem::get().update(); // update hot reloading
#endif
		ScriptManager::inst->update();
		idraw->pre_sync_update();
		if (get_level())
			get_level()->sync_level_render_data();
		UiSystem::inst->sync_to_renderer();
		g_physics.sync_render_data();
		idraw->sync_update();
	};
	auto wait_for_swap = [&](const bool skiprender) {
		// ZoneScopedN("SwapWindow");
		GPUSCOPESTART(gl_swap_window_scope);
		if (!(skip_swap || skiprender))
			gfx().submit_and_present();
	};

	double last = GetTime() - 0.1;
	// these are from the last game frame
	SceneDrawParamsEx drawparamsNext(0, 0);
	View_Setup setupNext;
	bool shouldDrawNext = true;

	for (;;) {
		try {
			gfx().begin_frame();

			// update time
			const double now = GetTime();
			double dt = now - last;
			last = now;
			if (dt > 0.1)
				dt = 0.1;
			frame_time = dt;

#ifdef EDITOR_BUILD
			if (test_runner) {
				if (test_runner->tick(dt)) {
					const int code = test_runner->exit_code();
					test_runner = nullptr;
					_exit(code);
				}
			}
#endif

			// update input, console cmd buffer, could change maps etc.
			frame_start();

#ifdef EDITOR_BUILD
			// In editor mode, if no tool got opened (e.g. via -open-editor in
			// init.txt, recents, or a script), open an empty map so the viewport
			// shows the editor's grey clear instead of the bare-framebuffer green.
			// Skip during tests — they manage their own editor doc lifecycle.
			if (is_editor_state() && !editor_tool && !test_runner && !is_test_mode()) {
				open_tool("<empty>");
			}
#endif

			const bool skip_rendering = !draw_this_frame();
			// overlapped update (game+render)
			do_overlapped_update(shouldDrawNext, drawparamsNext, setupNext, skip_rendering);

			// sync period
			imgui_render(skip_rendering);
			do_sync_update();
			// TracyGpuCollect;
			wait_for_swap(skip_rendering); // wait for swap last

			FrameMark;							  // tracy profiling
			Profiler::end_frame_tick(frame_time); // my crappy profilier
		}
		catch (LuaRuntimeError luaErr) {
			lua_error_loop(luaErr.what(), frame_start, wait_for_swap, drawparamsNext, setupNext);
		}
	}
}
