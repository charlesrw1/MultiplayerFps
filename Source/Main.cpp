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

MeshBuilder phys_debug;
Core core;

static double program_time_start;
static bool update_camera = false;
const char* map_file = "creek.glb";
static bool mouse_grabbed = false;

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
	ClientEntity* ent = ClientGetLocalPlayer();
	return ent->state.position;
	float alpha =  core.frame_remainder / core.tick_interval;
	return ent->prev_state.position * (1-alpha) + ent->state.position * ( alpha);
}

void ViewMgr::Update()
{
	if (!ClientIsInGame())
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
		ClientEntity* player = ClientGetLocalPlayer();
		vec3 front = AnglesToVector(client.view_angles.x, client.view_angles.y);
		fly_cam.position = GetLocalPlayerInterpedOrigin()+vec3(0,0.5,0) - front * 3.f;
		setup.view_mat = glm::lookAt(fly_cam.position, fly_cam.position + front, fly_cam.up);
		setup.viewproj = setup.proj_mat * setup.view_mat;
	}
	else
	{
		ClientEntity* player = ClientGetLocalPlayer();
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

void CreateWindow()
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

	core.window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
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
Texture* mytexture;
Model* m = nullptr;
Animator animator;
Model* gun;


const float DEFAULT_MOVESPEED = 0.1f;


float move_speed_player = 0.1f;
bool gravity = true;
bool col_response = true;
int col_iters = 2;
bool col_closest = true;
bool new_physics = true;

class PlayerPhysics
{
public:
	void Run(const Level* lvl, Entity* p, MoveCommand cmd, float dt);

private:
	void DoStandardPhysics();
	void ApplyFriction(float friction_val);
	void GroundMove();
	void AirMove();
	void FlyMove();
	void CheckJump();
	void CheckDuck();
	void CheckNans();
	void MoveAndSlide(vec3 delta);
	void CheckGroundState();

	const Level* level;
	float deltat;
	Entity* player;
	MoveCommand inp;
	vec2 inp_dir;
	float inp_len;

	int num_touch = 0;
	int touch_ents[16];	// index into client or server entities for all touches that occured
	// trace function callbacks
	void(*trace)(int) = nullptr;
	void(*impact_func)(int, int) = nullptr;

	vec3 look_front;
	vec3 look_side;
};


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

void Game::ExecutePlayerMove(Entity* ent, MoveCommand cmd)
{
	if (paused)
		return;
	phys_debug.Begin();
	PlayerPhysics physics;
	physics.Run(level,ent, cmd, core.tick_interval);
	phys_debug.End();
	DoGunLogic(ent, cmd);
	
}

void CreateMoveCommand()
{
	if (mouse_grabbed) {
		float x_off = core.input.mouse_delta_x;
		float y_off = core.input.mouse_delta_y;
		const float sensitivity = 0.01;
		x_off *= sensitivity;
		y_off *= sensitivity;

		glm::vec3 view_angles = client.view_angles;
		view_angles.x -= y_off;	// pitch
		view_angles.y += x_off;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);

		client.view_angles = view_angles;
	}
	MoveCommand new_cmd{};
	new_cmd.view_angles = client.view_angles;
	bool* keys = core.input.keyboard;
	if (keys[SDL_SCANCODE_W])
		new_cmd.forward_move += 1.f;
	if (keys[SDL_SCANCODE_S])
		new_cmd.forward_move -= 1.f;
	if (keys[SDL_SCANCODE_A])
		new_cmd.lateral_move += 1.f;
	if (keys[SDL_SCANCODE_D])
		new_cmd.lateral_move -= 1.f;
	if (keys[SDL_SCANCODE_SPACE])
		new_cmd.button_mask |= CmdBtn_Jump;
	if (keys[SDL_SCANCODE_LSHIFT])
		new_cmd.button_mask |= CmdBtn_Duck;
	if (keys[SDL_SCANCODE_Q])
		new_cmd.button_mask |= CmdBtn_Misc1;
	if (keys[SDL_SCANCODE_E])
		new_cmd.button_mask |= CmdBtn_Misc2;
	if (keys[SDL_SCANCODE_Z])
		new_cmd.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		new_cmd.up_move -= 1.f;

	new_cmd.tick = client.tick;

	*client.GetCommand(client.GetCurrentSequence()) = new_cmd;
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

static void DrawPlayer(mat4 viewproj)
{
	MeshBuilder mb;
	mb.Begin();
	EntityState interp = ClientGetLocalPlayer()->state;
	interp.position = GetLocalPlayerInterpedOrigin();
	
	DrawState(&interp, COLOR_GREEN, mb);

	for (int i = 0; i < client.cl_game.entities.size(); i++) {
		auto& ent = client.cl_game.entities[i];
		if (ent.state.type != Ent_Free && i != ClientGetPlayerNum()) {
			DrawState(&ent.state, COLOR_BLUE, mb);
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
	mb.Draw(GL_LINES);
	mb.Free();

	phys_debug.Draw(GL_LINES);
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
	mytexture = FindOrLoadTexture("test.png");
	gun = FindOrLoadModel("m16.glb");
	m = FindOrLoadModel("CT.glb");
	Shader::compile(&simple, "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&textured, "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&animated, "AnimBasicV.txt", "AnimBasicF.txt", "ANIMATED");
}

void Render(double dt)
{
	ViewSetup view = client.view_mgr.GetSceneView();
	//mat4 perspective = glm::perspective(fov, (float)vid_width / vid_height, 0.01f, 100.0f);
	//mat4 view = glm::lookAt(the_player.position - fly_cam.front * 2.f, the_player.position, vec3(0, 1, 0));
	glViewport(view.x, view.y, view.width, view.height);
	mat4 viewproj = view.viewproj;

	MeshBuilder mb;
	simple.use();
	simple.set_mat4("ViewProj", viewproj);
	simple.set_mat4("Model", mat4(1.f));
	DrawCollisionWorld(client.cl_game.level);
	mb.Begin();
	mb.PushLineBox(vec3(-1), vec3(1), COLOR_PINK);
	mb.End();
	mb.Draw(GL_LINES);
	DrawPlayer(viewproj);

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
	
#if 0
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
	DrawTempLevel(viewproj);
}

void DrawScreen(double dt)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	if (ClientIsInGame())
		Render(dt);

	SDL_GL_SwapWindow(core.window);
}


void DoClientGameUpdate(double dt)
{
	
}

void DoGameUpdate(double dt)
{
	
}

void RunClientPhysics(const PredictionState* in, PredictionState* out, const MoveCommand* cmd)
{
	PlayerPhysics physics;
	Entity ent;
	ent.ducking = in->estate.ducking;
	ent.position = in->estate.position;
	ent.rotation = in->estate.angles;
	ent.velocity = in->pstate.velocity;
	ent.on_ground = in->pstate.on_ground;
	ent.rotation = in->estate.angles;

	physics.Run(client.cl_game.level, &ent, *cmd, core.tick_interval);

	out->estate.ducking = ent.ducking;
	out->estate.position = ent.position;
	out->estate.angles = ent.rotation;

	out->pstate.on_ground = ent.on_ground;
	out->pstate.velocity = ent.velocity;
	out->estate.angles = ent.rotation;
}

void DoClientPrediction()
{
	if (ClientGetState() != Spawned)
		return;
	// predict commands from outgoing ack'ed to current outgoing
	// TODO: dont repeat commands unless a new snapshot arrived
	int start = client.server_mgr.server.out_sequence_ak;	// start at the new cmd
	int end = client.server_mgr.server.out_sequence;
	int commands_to_run = end - start;
	if (commands_to_run > CLIENT_MOVE_HISTORY)	// overflow
		return;
	// restore state to last authoritative snapshot 
	int incoming_seq = client.server_mgr.server.in_sequence;
	Snapshot* last_auth_state = &client.cl_game.snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);
	PredictionState pred_state;
	pred_state.estate = last_auth_state->entities[client.server_mgr.client_num];
	pred_state.pstate = last_auth_state->pstate;
	PredictionState next_state;
	for (int i = start+1; i < end; i++) {
		MoveCommand* cmd = client.GetCommand(i);
		RunClientPhysics(&pred_state, &next_state, cmd);
		pred_state = next_state;
	}
	
	ClientEntity* ent = ClientGetLocalPlayer();
	ent->state = pred_state.estate;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).tick = client.tick;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).origin = ent->state.position;
	//ent->current_hist_index++;
	//ent->current_hist_index %= ent->transform_hist.size();
}

void CheckLocalServerRunning()
{
	if (ClientGetState() == Disconnected && ServerIsActive()) {
		// connect to the local server
		IPAndPort serv_addr;
		serv_addr.SetIp(127, 0, 0, 1);
		serv_addr.port = SERVER_PORT;
		client.server_mgr.Connect(serv_addr);
	}
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
				ClientDisconnect();
			if (scancode == SDL_SCANCODE_2)
				ClientReconnect();
			if (scancode == SDL_SCANCODE_3)
				ServerEnd();
			if (scancode == SDL_SCANCODE_4)
				ServerSpawn(map_file_names[map_selection]);
			if (scancode == SDL_SCANCODE_5)
				core.tick_interval = 1.0 / 15;
			if (scancode == SDL_SCANCODE_6)
				core.tick_interval = 1.0 / 66.66;
			if (scancode == SDL_SCANCODE_7) {
				IPAndPort ip;
				ip.SetIp(127, 0, 0, 1);
				ip.port = SERVER_PORT;
				ClientConnect(ip);
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
			mouse_grabbed = true;
			update_camera = true;
			break;
		case SDL_MOUSEBUTTONUP:
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_grabbed = false;
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

void ClientFixedUpdateInput(double dt)
{
	if (!client.initialized)
		return;
	// make command and run prediction
	if (ClientGetState() >= Connected) {
		CreateMoveCommand();
	}
	client.server_mgr.SendMovesAndMessages();
	//printf("tick %d\n", client.tick);
	CheckLocalServerRunning();
	client.server_mgr.TrySendingConnect();
}
void ClientFixedUpdateRead(double dt)
{
	if (!client.initialized)
		return;
	if (ClientGetState() == Spawned)
		client.tick += 1;
	client.server_mgr.ReadPackets();
	DoClientPrediction();
}
void ServerFixedUpdate(double dt)
{
	if (!server.active)
		return;
	server.client_mgr.ReadPackets();
	DoGameUpdate(dt);
	server.client_mgr.SendSnapshots();

	server.tick += 1;
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
		ClientFixedUpdateInput(secs_per_tick);
		ServerFixedUpdate(secs_per_tick);
		ClientFixedUpdateRead(secs_per_tick);
	}
	
	client.view_mgr.Update();
	DoClientGameUpdate(frametime);
	DrawScreen(frametime);
}

static bool run_client_only = false;

int main(int argc, char** argv)
{
	printf("Starting game\n");
	if (argc == 2 && strcmp(argv[1], "-client") == 0)
		run_client_only = true;
	CreateWindow();
	NetworkInit();
	RenderInit();
	if (!run_client_only) {
		ServerInit();
		ServerSpawn(map_file);
	}
	ClientInit();
	if (run_client_only) {
		printf("Running client only\n");
		IPAndPort ip;
		ip.SetIp(127, 0, 0, 1);
		ip.port = SERVER_PORT;
		ClientConnect(ip);
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

		//if (delta_t > 0.1)
		//	delta_t = 0.1;
		//TEMP_DT = delta_t;
		//HandleInput();
		//ClientUpdate(delta_t);
		//ServerUpdate(delta_t);
		//if (client.initialized) {
		//	client.view_mgr.Update();
		//	DrawScreen(delta_t);
		//}
	}
	
	return 0;
}