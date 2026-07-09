#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
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
#include "imgui_internal.h" // ErrorCheckEndFrameRecover -- see lua_error_loop
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
#include "Framework/Jobs.h"
#include "EditorPopups.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Framework/StringUtils.h"
#include "EngineMain.h"
#include "IntegrationTests/TestRunner.h"
#include "Framework/Util.h"
#include "Framework/ProfilerUI.h"
#include <mutex>

#include "Logging.h"

extern GameEngineLocal eng_local;
extern ConfigVar g_window_fullscreen;
extern ConfigVar g_engine_ticks_per_frame;
extern ConfigVar g_slomo;
extern ConfigVar stat_fps;

// Free functions defined in EngineMain_Debug.cpp
extern void debug_shape_ctx_update(float dt);
extern void debug_shape_ctx_fixed_update_start();

// Unreal-style "stat fps" overlay: current framerate and frametime in the top-right
// corner, colored by how close the frame came to a 60/30 fps budget.
static void draw_fps_counter(double dt) {
	if (!stat_fps.get_bool())
		return;

	auto font = g_assets.find<GuiFont>("eng/fonts/monospace12.fnt").get();
	if (!font)
		return;

	const float ms = (float)(dt * 1000.0);
	const float fps = dt > 0.0 ? (float)(1.0 / dt) : 0.f;

	char buf[64];
	snprintf(buf, sizeof(buf), "%.0f fps  %.1f ms", fps, ms);

	Color32 color;
	if (ms <= 16.7f) // 60+ fps
		color = Color32(60, 230, 60, 255);
	else if (ms <= 33.3f) // 30-60 fps
		color = Color32(230, 200, 40, 255);
	else // <30 fps
		color = Color32(230, 50, 50, 255);

	Rect2d size = GuiHelpers::calc_text_size_no_wrap(std::string_view(buf), font);
	glm::ivec2 pos = GuiHelpers::calc_layout({-size.w - 12, 10}, guiAnchor::TopRight, UiSystem::inst->get_vp_rect());

	TextShape shape;
	shape.rect = Rect2d(pos, {0, 0});
	shape.font = font;
	shape.text = buf;
	shape.color = color;
	shape.with_drop_shadow = true;
	shape.drop_shadow_ofs = 1;
	UiSystem::inst->window.draw(shape);
}

struct GameUpdateOuput
{
	bool drawOut = false;
	SceneDrawParamsEx paramsOut = SceneDrawParamsEx(0, 0);
	View_Setup vsOut;
};

static void lua_error_loop(string msg, auto&& frame_start, auto&& wait_for_swap) {
	// bsod is funny lol
	sys_print(Error, "loop: caught LuaRuntimeError: %s\n", msg.c_str());
	auto lines = StringUtils::to_lines(msg);

	// Deliberately don't go through the normal scene_draw / editor viewport / property-panel
	// path here:
	//  - In the editor, the game view renders into an offscreen texture that only reaches the
	//    screen via the editor's own docked ImGui panel (UiSystem::draw_imgui_interfaces).
	//    Reusing the last frame's SceneDrawParamsEx/View_Setup and calling idraw->scene_draw
	//    directly (the old approach) therefore never actually reached the presented backbuffer
	//    -- nothing appeared, which is why no BSOD showed up.
	//  - Routing through the editor's panels instead would re-run property-grid/preview code,
	//    which is what threw in the first place (Component::editor_start), causing it to throw
	//    again every ~10ms -- the actual source of the "flicker".
	// Drawing our own minimal, opaque, full-viewport ImGui window and pushing it straight to
	// gfx().imgui_render_draw_data() sidesteps both problems: it touches none of the
	// game/editor state that's implicated in the error, and ImGui always composites directly
	// onto the real presented backbuffer regardless of editor/game mode.
	while (1) {
		SDL_Delay(10);

		// Quitting from here must not go through frame_start()'s normal ::Quit() path: that
		// calls eng_local.cleanup() -> stop_game() -> Component::stop() on every live
		// component, including whichever one has the still-broken Lua state that put us on
		// this screen in the first place. That re-throws right back into this loop's own
		// nested-LuaRuntimeError catch below, which just prints and loops again -- the
		// exit(0) in Quit() never runs, so the close button silently does nothing. Peek (not
		// consume, so ImGui/Input still see it) for the quit event and hard-exit before any
		// cleanup gets a chance to touch the broken script.
		if (SDL_HasEvent(SDL_EVENT_QUIT)) {
			sys_print(Info, "loop: quit requested while showing Lua error screen, exiting without cleanup\n");
			exit(0);
		}

		gfx().begin_frame();

		// A script can also error out from console-command handling, hot-reload, or other
		// code paths reachable from frame_start() below. Catch it here instead of letting it
		// escape this loop uncaught -- that would unwind past the try/catch in loop() and
		// terminate the app, which looks like a freeze/crash right as the user is trying to
		// fix the original error.
		try {
			frame_start();

			if (Input::was_key_pressed(SDL_SCANCODE_RETURN))
				break;

			// The throw that got us here (or a nested one caught below) may have happened
			// mid-way through the editor's own ImGui drawing (e.g. a property panel refresh
			// that calls back into a script while inside Begin("Properties")). That leaves
			// ImGui's window/style/ID stacks with unmatched Begin/PushStyleVar/etc calls, and
			// its NewFrame() itself left dangling without a matching Render()/EndFrame().
			// ErrorCheckEndFrameRecover() force-closes everything left open (a no-op if the
			// last frame actually closed cleanly), then EndFrame() is safe to call.
			ImGui::ErrorCheckEndFrameRecover(nullptr);
			ImGui::EndFrame();

			gfx().imgui_new_frame();
			ImGui::NewFrame();

			ImGuiViewport* vp = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(vp->Pos);
			ImGui::SetNextWindowSize(vp->Size);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.25f, 1.0f));
			ImGui::Begin("##lua_bsod", nullptr,
						  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
							  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "LUA RUNTIME ERROR");
			ImGui::Separator();
			for (auto& l : lines)
				ImGui::TextUnformatted(l.c_str());
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Fix the script and save, then press ENTER to continue.");
			ImGui::End();
			ImGui::PopStyleColor();

			ImGui::Render();
			gfx().imgui_render_draw_data();
			wait_for_swap(false);
		}
		catch (LuaRuntimeError& nested) {
			sys_print(Error, "loop: caught LuaRuntimeError while showing error screen: %s\n", nested.what());
			lines = StringUtils::to_lines(nested.what());
		}
	}
	ScriptManager::inst->check_for_reload();
}

void GameEngineLocal::game_update_tick() {
	CPU_SCOPE("game_update_tick");

	auto fixed_update = [&](double dt) {
		CPU_SCOPE("fixed_update");

		debug_shape_ctx_fixed_update_start();
		g_physics.simulate_and_fetch(dt);
	};
	auto update = [&](double dt) {
		CPU_SCOPE("update");
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

	// Step the fixed-timestep physics using the accumulator computed above, so dynamic
	// physics advances real time regardless of render fps (previously hardcoded to a single
	// step per frame, which ran pure-dynamic sims like ragdolls in slow motion whenever the
	// editor/game dipped below the tick rate). frame_remainder was already drained by the full
	// num_ticks, so clamping steps here just discards backlog on a hitch (anti spiral-of-death).
	int steps = num_ticks;
	if (steps < 0)
		steps = 0;
	if (steps > 5)
		steps = 5;
	for (int i = 0; i < steps; i++) {
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
		CPU_SCOPE("frame_start");
		// update input
		Input::inst->pre_events();
		UiSystem::inst->pre_events();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			gfx().imgui_process_event(&event);
			Input::inst->handle_event(event);
			UiSystem::inst->handle_event(event);

			switch (event.type) {
			case SDL_EVENT_QUIT:
				::Quit();
				break;
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
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
				break;
			}
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_WHEEL:
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

	auto wait_for_swap = [&](const bool skiprender) {
		// SwapWindow blocks the CPU thread on vsync -- it must be a CPU_SCOPE
		// (not just GPU_SCOPE) so the Main thread's frame span includes the
		// wait, otherwise the profiler's FPS/frame-time reads high (CPU-busy
		// time only) while stat.fps (measured wall-clock) reads correctly.
		RENDER_SCOPE("SwapWindow");
		if (!(skip_swap || skiprender))
			gfx().submit_and_present();
	};

	auto do_overlapped_update = [&](bool& shouldDrawNext, SceneDrawParamsEx& drawparamsNext, View_Setup& setupNext,
									const bool skip_rendering, bool& prev_skip_rendering) {
		CPU_SCOPE("OverlappedUpdate");
		BooleanScope scope(b_is_in_overlapped_period);
		GameUpdateOuput out;

		// I reworked the asset system so have to disable this for now. issue is sync loading assets on game thread.
		// otherwise everything else is threadsafe(tm).
		//
		// JobCounter* gameupdatecounter{};
		// JobSystem::inst->add_job(game_update_job,uintptr_t(&out), gameupdatecounter);
		// game_update_job(uintptr_t(&out));

		{
			CPU_SCOPE("GameThreadUpdate");

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

		// Present the PREVIOUS frame's already-submitted GPU commands now,
		// right after this frame's CPU-only game update -- that update is
		// real work for the GPU's remaining render time to overlap with,
		// instead of blocking on swap only after also submitting this
		// frame's commands (which leaves the CPU idle for the whole wait).
		// Costs one extra frame of submit-to-present latency in exchange.
		wait_for_swap(prev_skip_rendering);
		prev_skip_rendering = skip_rendering;

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
		RENDER_SCOPE("ImGuiUpdate");

		gui_log.draw(UiSystem::inst->window);
		draw_fps_counter(frame_time);

		UiSystem::inst->draw_imgui_interfaces(editor_tool.get());
		prof_ui::draw();

		ImGui::Render();
		if (!skip_rendering) {
			gfx().imgui_render_draw_data();
		}
	};
	auto do_sync_update = [&]() {
		CPU_SCOPE("SyncUpdate");
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

	double last = GetTime() - 0.1;
	// these are from the last game frame
	SceneDrawParamsEx drawparamsNext(0, 0);
	View_Setup setupNext;
	bool shouldDrawNext = true;
	bool prev_skip_rendering = true; // nothing submitted yet on the first iteration

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
			// overlapped update: game update, then present the previous
			// frame (see wait_for_swap call inside), then submit this
			// frame's scene_draw commands.
			do_overlapped_update(shouldDrawNext, drawparamsNext, setupNext, skip_rendering, prev_skip_rendering);

			// sync period
			imgui_render(skip_rendering);
			do_sync_update();

			if (!is_test_mode()) {
				auto flags = SDL_GetWindowFlags(window);
				if (!(flags & SDL_WINDOW_INPUT_FOCUS))
					SDL_Delay(200);
			}

			prof::Profiler::end_frame();
		}
		catch (LuaRuntimeError& luaErr) {
			lua_error_loop(luaErr.what(), frame_start, wait_for_swap);
		}
	}
}
