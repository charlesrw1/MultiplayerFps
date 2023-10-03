#include <SDL2/SDL.h>
#include "glad/glad.h"
#include "stb_image.h"
#include <cstdio>
#include <vector>
#include <string>

#include "Shader.h"
#include "Texture.h"
#include "MathLib.h"
#include "GlmInclude.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Model.h"
#include "MeshBuilder.h"
#include "Util.h"
#include "Animation.h"
#include "Level.h"
#include "Physics.h"
#include "Net.h"
#include "CoreTypes.h"
#include "Client.h"
#include "Server.h"
#include "Movement.h"

MeshBuilder phys_debug;
Core core;

struct Media
{
	Model* playermod;
	Model* gun;
	Texture* testtex;

	Shader simple;
	Shader textured;
	Shader animated;
};

static Media media;

static double program_time_start;
static bool update_camera = false;
const char* map_file = "test_level2.glb";

const char* map_file_names[] = { "creek.glb","maze.glb","nuke.glb" };

bool CheckGlErrorInternal_(const char* file, int line)
{
	GLenum error_code = glGetError();
	bool has_error = 0;
	while (error_code != GL_NO_ERROR)
	{
		has_error = true;
		const char* error_name = "Unknown error";
		switch (error_code)
		{
		case GL_INVALID_ENUM:
			error_name = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE:
			error_name = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:
			error_name = "GL_INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:
			error_name = "GL_STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:
			error_name = "GL_STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:
			error_name = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default:
			break;
		}
		printf("%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
}

bool IsServerActive()
{
	return server.IsActive();
}
bool IsClientActive()
{
	return client.initialized;
}


static void Cleanup()
{
	FreeLoadedModels();
	FreeLoadedTextures();
	NetworkQuit();
	SDL_GL_DeleteContext(core.context);
	SDL_DestroyWindow(core.window);
	SDL_Quit();
}
void Quit()
{
	printf("Quiting...\n");
	Cleanup();
	exit(0);
}
void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	fflush(stdout);
	Cleanup();
	exit(-1);
}
double GetTime()
{
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart()
{
	return GetTime() - program_time_start;
}


void FlyCamera::UpdateVectors() {
	front = AnglesToVector(pitch, yaw);
}
glm::mat4 FlyCamera::GetViewMatrix() const {
	return glm::lookAt(position, position + front, up);
}

void ViewMgr::Init()
{
	setup.height = core.vid_height;
	setup.width = core.vid_width;
	setup.viewfov = fov;
	setup.x = setup.y = 0;
	setup.proj_mat = glm::perspective(setup.viewfov, (float)setup.width / setup.height, z_near, z_far);
}

vec3 GetLocalPlayerInterpedOrigin()
{
	ClientEntity* ent = client.GetLocalPlayer();
	return ent->state.position;
	float alpha =  core.frame_remainder / core.tick_interval;
	return ent->prev_state.position * (1-alpha) + ent->state.position * ( alpha);
}

void ViewMgr::Update()
{
	if (!client.IsInGame())
		return;
	setup.height = core.vid_height;
	setup.width = core.vid_width;
	setup.viewfov = fov;
	setup.x = setup.y = 0;
	setup.proj_mat = glm::perspective(setup.viewfov, (float)setup.width / setup.height, z_near, z_far);
	Core::InputState& input = core.input;
	if (update_camera) {
		fly_cam.UpdateFromInput(input.keyboard, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	}
	if (third_person) {
		ClientEntity* player = client.GetLocalPlayer();
		vec3 front = AnglesToVector(client.view_angles.x, client.view_angles.y);
		fly_cam.position = GetLocalPlayerInterpedOrigin()+vec3(0,0.5,0) - front * 3.f;
		setup.view_mat = glm::lookAt(fly_cam.position, fly_cam.position + front, fly_cam.up);
		setup.viewproj = setup.proj_mat * setup.view_mat;
	}
	else
	{
		ClientEntity* player = client.GetLocalPlayer();
		EntityState* pstate = &player->state;
		float view_height = (pstate->ducking) ? 0.65 : 1.2;
		vec3 cam_position = pstate->position + vec3(0, view_height, 0);
		vec3 front = AnglesToVector(client.view_angles.x,client.view_angles.y);
		setup.view_mat = glm::lookAt(cam_position, cam_position + front, vec3(0, 1, 0));
		setup.viewproj = setup.proj_mat * setup.view_mat;
		setup.vieworigin = cam_position;
		setup.viewfront = front;
	}
}

static void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}

void CreateWindow(int x, int y)
{
	if (core.window)
		return;

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		SDLError("SDL init failed");
	}

	program_time_start = GetTime();

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	core.window = SDL_CreateWindow("CsRemake", x, y, 
		core.vid_width, core.vid_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (core.window == nullptr) {
		SDLError("SDL failed to create window");
	}

	core.context = SDL_GL_CreateContext(core.window);
	printf("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);
}

void FlyCamera::UpdateFromInput(const bool keys[], int mouse_dx, int mouse_dy, int scroll)
{
	int xpos, ypos;
	xpos = mouse_dx;
	ypos = mouse_dy;

	float x_off = xpos;
	float y_off = ypos;
	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;

	yaw += x_off;
	pitch -= y_off;

	if (pitch > HALFPI - 0.01)
		pitch = HALFPI - 0.01;
	if (pitch < -HALFPI + 0.01)
		pitch = -HALFPI + 0.01;

	if (yaw > TWOPI) {
		yaw -= TWOPI;
	}
	if (yaw < 0) {
		yaw += TWOPI;
	}
	UpdateVectors();

	vec3 right = cross(up, front);
	if (keys[SDL_SCANCODE_W])
		position += move_speed * front;
	if (keys[SDL_SCANCODE_S])
		position -= move_speed * front;
	if (keys[SDL_SCANCODE_A])
		position += right * move_speed;
	if (keys[SDL_SCANCODE_D])
		position -= right * move_speed;
	if (keys[SDL_SCANCODE_Z])
		position += move_speed * up;
	if (keys[SDL_SCANCODE_X])
		position -= move_speed * up;

	move_speed += (move_speed * 0.5) * scroll;
	if (abs(move_speed) < 0.000001)
		move_speed = 0.0001;
}

Shader simple;
Shader textured;
Shader animated;
Shader static_wrld;


const Capsule DEFAULT_COLLIDER = {
	0.2f, glm::vec3(0,0,0),glm::vec3(0,1.3,0.0)
};
const Capsule CROUCH_COLLIDER = {
	0.2f, glm::vec3(0,0,0),glm::vec3(0,0.75,0.0)
};
const float DEFAULT_MOVESPEED = 0.1f;


static float move_speed_player = 0.1f;
static bool gravity = true;
static bool col_response = true;
static int col_iters = 2;
static bool col_closest = true;
static bool new_physics = true;

static float ground_friction = 10;
static float air_friction = 0.01;
static float gravityamt = 16;
static float ground_accel = 6;
static float air_accel = 3;
static float minspeed = 1;
static float maxspeed = 30;
static float jumpimpulse = 5.f;

void ShootGunAgainstWorld(ColliderCastResult* result, Ray r)
{

}


void DoGunLogic(Entity* ent, MoveCommand cmd)
{
	if (cmd.button_mask & CmdBtn_Misc1 && ent->next_shoot_time <= server.time) {
		Ray gun_ray;
		gun_ray.dir = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);
		gun_ray.pos = ent->position + vec3(0, 0.5, 0);
	}
}


void Server_TraceCallback(ColliderCastResult* out, PhysContainer obj, bool closest, bool double_sided)
{
	TraceAgainstLevel(server.sv_game.level, out, obj, closest, double_sided);
}

void EntToPlayerState(PlayerState* state, Entity* ent)
{
	state->position = ent->position;
	state->angles = ent->view_angles;
	state->ducking = ent->ducking;
	state->on_ground = ent->on_ground;
	state->velocity = ent->velocity;
}
void PlayerStateToEnt(Entity* ent, PlayerState* state)
{
	ent->position = state->position;
	ent->rotation = state->angles;
	//
	ent->ducking = state->ducking;
	ent->on_ground = state->on_ground;
	ent->velocity = state->velocity;
}


void Game::ExecutePlayerMove(Entity* ent, MoveCommand cmd)
{
	//if (paused)
	//	return;
	//phys_debug.Begin();
	//PlayerPhysics physics;
	//physics.Run(level,ent, cmd, core.tick_interval);
	//phys_debug.End();

	phys_debug.Begin();
	PlayerMovement move;
	move.cmd = cmd;
	move.deltat = core.tick_interval;
	move.phys_debug = &phys_debug;
	move.trace_callback = Server_TraceCallback;
	EntToPlayerState(&move.in_state, ent);
	move.Run();
	PlayerStateToEnt(ent, move.GetOutputState());
	phys_debug.End();
	

	DoGunLogic(ent, cmd);
	
}
void DrawTempLevel(mat4 viewproj)
{
	if (static_wrld.ID == 0)
		Shader::compile(&static_wrld, "AnimBasicV.txt", "AnimBasicF.txt","VERTEX_COLOR");
	static_wrld.use();
	static_wrld.set_mat4("ViewProj", viewproj);

	const Level* level = client.cl_game.level;
	for (int m = 0; m < level->render_data.instances.size(); m++) {
		const Level::StaticInstance& sm = level->render_data.instances[m];
		Model* model = level->render_data.embedded_meshes[sm.model_index];
		static_wrld.set_mat4("Model", sm.transform);
		static_wrld.set_mat4("InverseModel", glm::inverse(sm.transform));
		for (int p = 0; p < model->parts.size(); p++) {
			MeshPart& mp = model->parts[p];
			glBindVertexArray(mp.vao);
			glDrawElements(GL_TRIANGLES, mp.element_count, mp.element_type, (void*)mp.element_offset);
		}
	}
}

static void DrawState(EntityState* player, Color32 color, MeshBuilder& mb)
{
	vec3 origin = player->position;
	const Capsule& c = (player->ducking) ? CROUCH_COLLIDER : DEFAULT_COLLIDER;
	float radius = c.radius;
	vec3 a, b;
	c.GetSphereCenters(a, b);
	mb.AddSphere(origin + a, radius, 10, 7, color);
	mb.AddSphere(origin + b, radius, 10, 7, color);
}

static void DrawInterpolatedEntity(ClientEntity* cent, Color32 c, MeshBuilder* mb, mat4 viewproj)
{
	glCheckError();

	EntityState* cur = &cent->state;
	Animator ca;
	ca.Init(media.playermod);
	ca.ResetLayers();
	ca.mainanim = cur->leganim;
	ca.mainanim_frame = cur->leganim_frame;

	DrawState(cur, c, *mb);

	glCheckError();

	animated.use();
	animated.set_mat4("ViewProj", viewproj);

	mat4 model = glm::translate(mat4(1), cur->position);
	model = model*glm::eulerAngleXYZ(cur->angles.x, cur->angles.y, cur->angles.z);
	model = glm::scale(model, vec3(0.018f));

	animated.set_mat4("Model", model);
	animated.set_mat4("InverseModel", mat4(1));
	
	glCheckError();

	ca.SetupBones();
	ca.ConcatWithInvPose();
	const std::vector<mat4>& bones = ca.GetBones();

	const uint32_t bone_matrix_loc = glGetUniformLocation(animated.ID, "BoneTransform[0]");

	glCheckError();

	for (int j = 0; j < bones.size(); j++)
		glUniformMatrix4fv(bone_matrix_loc + j, 1, GL_FALSE, glm::value_ptr(bones[j]));
	glCheckError();


	//glDisable(GL_CULL_FACE);
	for (int i = 0; i < media.playermod->parts.size(); i++)
	{
		MeshPart* part = &media.playermod->parts[i];
		glBindVertexArray(part->vao);
		glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}
	glCheckError();
}


static void DrawWorldEnts(mat4 viewproj)
{
	MeshBuilder mb;
	mb.Begin();

	glCheckError();

	for (int i = 0; i < client.cl_game.entities.size(); i++) {
		auto& ent = client.cl_game.entities[i];
		if (ent.state.type != Ent_Free && (i != client.GetPlayerNum()|| client.view_mgr.third_person)) {
			DrawInterpolatedEntity(&ent, COLOR_BLUE, &mb, viewproj);
		}
	}

	if (server.active) {
		vec3 origin2 = ServerEntForIndex(0)->position;
		const Capsule& c2 = (ServerEntForIndex(0)->ducking) ? CROUCH_COLLIDER : DEFAULT_COLLIDER;
		float radius = c2.radius;
		vec3 a, b;
		c2.GetSphereCenters(a, b);

		mb.AddSphere(origin2 + a, radius, 10, 7, COLOR_CYAN);
		mb.AddSphere(origin2 + b, radius, 10, 7, COLOR_CYAN);
	}
	mb.End();
	
	simple.use();
	simple.set_mat4("ViewProj", viewproj);
	simple.set_mat4("Model", mat4(1.f));
	
	mb.Draw(GL_LINES);
	mb.Free();

	if(IsServerActive())
		phys_debug.Draw(GL_LINES);
}

static void DrawLocalPlayer(mat4 viewproj)
{

}


void InitGlState()
{
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);
	glEnable(GL_MULTISAMPLE);
	glDepthFunc(GL_LEQUAL);
}


void RenderInit()
{
	InitGlState();
	media.gun = FindOrLoadModel("m16.glb");
	media.playermod = FindOrLoadModel("CT.glb");
	media.testtex = FindOrLoadTexture("test.png");

	Shader::compile(&simple, "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&textured, "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&animated, "AnimBasicV.txt", "AnimBasicF.txt", "ANIMATED");
}

void Render(double dt)
{
	glCheckError();

	ViewSetup view = client.view_mgr.GetSceneView();
	//mat4 perspective = glm::perspective(fov, (float)vid_width / vid_height, 0.01f, 100.0f);
	//mat4 view = glm::lookAt(the_player.position - fly_cam.front * 2.f, the_player.position, vec3(0, 1, 0));
	glViewport(view.x, view.y, view.width, view.height);
	mat4 viewproj = view.viewproj;

	glCheckError();

	MeshBuilder mb;
	simple.use();
	simple.set_mat4("ViewProj", viewproj);
	simple.set_mat4("Model", mat4(1.f));
	DrawCollisionWorld(client.cl_game.level);
	mb.Begin();
	mb.PushLineBox(vec3(-1), vec3(1), COLOR_PINK);
	mb.End();
	mb.Draw(GL_LINES);
	mb.Free();

	glCheckError();
	

	DrawWorldEnts(viewproj);
	glCheckError();

#if 0
	textured.use();
	textured.set_mat4("ViewProj", viewproj);
	textured.set_mat4("Model", mat4(1));
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mytexture->gl_id);

	mb.Begin();
	mb.Push2dQuad(vec2(-1),vec2(2),vec2(0),vec2(1),COLOR_WHITE);
	mb.End();
	mb.Draw(GL_TRIANGLES);
	mb.Free();
	
	// Update animation
	animator.AdvanceFrame(dt);
	animator.SetupBones();
	animator.ConcatWithInvPose();
	//

	animated.use();
	animated.set_mat4("ViewProj", viewproj);
	animated.set_mat4("Model", mat4(1));
	animated.set_mat4("InverseModel", mat4(1));

	const std::vector<mat4>& bones = animator.GetBones();

	const uint32_t bone_matrix_loc = glGetUniformLocation(animated.ID, "BoneTransform[0]");

	for (int j = 0; j < bones.size(); j++)
		glUniformMatrix4fv(bone_matrix_loc + j, 1, GL_FALSE, glm::value_ptr(bones[j]));
	glCheckError();


	//glDisable(GL_CULL_FACE);
	for (int i = 0; i < m->parts.size(); i++)
	{
		MeshPart* part = &m->parts[i];
		glBindVertexArray(part->vao);
		glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}
	glCheckError();

#endif
	glCheckError();
	DrawTempLevel(viewproj);
	glCheckError();
}

void DrawScreen(double dt)
{
	glCheckError();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	if (client.IsInGame())
		Render(dt);
	glCheckError();
	SDL_GL_SwapWindow(core.window);
	glCheckError();

}


bool using_arrows_to_select_map = false;
int map_selection = 0;

bool DoMapSelect(SDL_Scancode key)
{
	if (key == SDL_SCANCODE_SLASH) {
		using_arrows_to_select_map = !using_arrows_to_select_map;
		printf("selecting map: %s\n", (using_arrows_to_select_map) ? "on" : "off");
		return true;
	}
	if (!using_arrows_to_select_map)
		return false;
	int map_select_old = map_selection;
	if (key == SDL_SCANCODE_LEFT)
		map_selection--;
	if (key == SDL_SCANCODE_RIGHT)
		map_selection++;
	if (map_selection < 0) map_selection = 0;
	else if (map_selection >= sizeof(map_file_names) / sizeof(char*)) map_selection = (sizeof(map_file_names) / sizeof(char*)-1);
	if (map_select_old != map_selection) {
		printf("map selected: %s\n", map_file_names[map_selection]);
	}
	return true;
}

void HandleInput()
{
	Core::InputState& input = core.input;
	input.mouse_delta_x = input.mouse_delta_y = input.scroll_delta = 0;
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			Quit();
			break;
		case SDL_KEYDOWN:
		{
			auto scancode = event.key.keysym.scancode;
			if (!DoMapSelect(scancode)) {

			int col_iter_old = col_iters;
			if (scancode == SDL_SCANCODE_LEFT)
				col_iters--;
			if (scancode == SDL_SCANCODE_RIGHT)
				col_iters++;
			if (col_iters <= 0) col_iters = 1;
			if (col_iters != col_iter_old)
				printf("col iters %d\n", col_iters);
			}
			if (scancode == SDL_SCANCODE_G) {
				gravity = !gravity;
				printf("gravity %s\n", (gravity) ? "on" : "off");
			}
			if (scancode == SDL_SCANCODE_C) {
				col_response = !col_response;
				printf("col_response %s\n", (col_response) ? "on" : "off");
			}
			if (scancode == SDL_SCANCODE_V) {
				col_closest = !col_closest;
				printf("col_closest %s\n", (col_closest) ? "on" : "off");
			}
			if (scancode == SDL_SCANCODE_APOSTROPHE) {
				new_physics = !new_physics;
			}
			if (scancode == SDL_SCANCODE_T)
				client.view_mgr.third_person = !client.view_mgr.third_person;
			if (scancode == SDL_SCANCODE_Q)
				server.sv_game.paused = !server.sv_game.paused;
			if(scancode == SDL_SCANCODE_1)
				client.Disconnect();
			if (scancode == SDL_SCANCODE_2)
				client.Reconnect();
			if (scancode == SDL_SCANCODE_3)
				server.End();
			if (scancode == SDL_SCANCODE_4)
				server.Spawn(map_file_names[map_selection]);
			if (scancode == SDL_SCANCODE_5)
				core.tick_interval = 1.0 / 15;
			if (scancode == SDL_SCANCODE_6)
				core.tick_interval = 1.0 / 66.66;
			if (scancode == SDL_SCANCODE_7) {
				IPAndPort ip;
				ip.SetIp(127, 0, 0, 1);
				ip.port = SERVER_PORT;
				client.Connect(ip);
			}

		}
		case SDL_KEYUP:
			input.keyboard[event.key.keysym.scancode] = event.key.type == SDL_KEYDOWN;
			break;
		case SDL_MOUSEMOTION:
			input.mouse_delta_x += event.motion.xrel;
			input.mouse_delta_y += event.motion.yrel;
			break;
		case SDL_MOUSEWHEEL:
			input.scroll_delta += event.wheel.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			SDL_SetRelativeMouseMode(SDL_TRUE);
			core.mouse_grabbed = true;
			update_camera = true;
			break;
		case SDL_MOUSEBUTTONUP:
			SDL_SetRelativeMouseMode(SDL_FALSE);
			core.mouse_grabbed = false;
			update_camera = false;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				core.vid_height = event.window.data2;
				core.vid_width = event.window.data1;
			}
			break;

		}
	}
}


void MainLoop(double frametime)
{
	double secs_per_tick = core.tick_interval;
	core.frame_remainder += frametime;
	int num_ticks = (int)floor(core.frame_remainder / secs_per_tick);
	core.frame_remainder -= num_ticks * secs_per_tick;
	for (int i = 0; i < num_ticks; i++)
	{
		HandleInput();
		client.FixedUpdateInput(secs_per_tick);
		server.FixedUpdate(secs_per_tick);
		client.FixedUpdateRead(secs_per_tick);
	}
	
	client.PreRenderUpdate(frametime);
	DrawScreen(frametime);
}

static bool run_client_only = false;
int main(int argc, char** argv)
{
	printf("Starting game\n");

	int starth = DEFAULT_HEIGHT;
	int startw = DEFAULT_WIDTH;
	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-client") == 0)
			run_client_only = true;
		else if (strcmp(argv[i], "-x") == 0) {
			startx = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-y") == 0) {
			starty = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			startw = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-h") == 0) {
			starth = atoi(argv[++i]);
		}
	}
	core.vid_width = startw;
	core.vid_height = starth;


	CreateWindow(startx,starty);
	NetworkInit();
	RenderInit();
	if (!run_client_only) {
		server.Init();
		server.Spawn(map_file);
	}
	client.Init();
	if (run_client_only) {
		printf("Running client only\n");
		IPAndPort ip;
		ip.SetIp(127, 0, 0, 1);
		ip.port = SERVER_PORT;
		client.Connect(ip);
	}
	core.tick_interval = 1.0 / DEFAULT_UPDATE_RATE;	// hardcoded
	double last = GetTime()-0.1;
	for (;;)
	{
		double now = GetTime();
		double dt = now - last;
		last = now;
		if (dt > 0.1)
			dt = 0.1;
		core.frame_time = dt;
		MainLoop(dt);

		static int next_print = 0.0;
		if (next_print < GetTime()) {
			printf("frame %f\n", dt);
			next_print = GetTime() + 1.0;
		}
	}
	
	return 0;
}