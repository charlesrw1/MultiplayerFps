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
#include "Entity.h"
#include "Net.h"
#include "CoreTypes.h"

MeshBuilder phys_debug;
Core core;

static double program_time_start;
static bool update_camera = false;
const char* map_file = "maze.glb";
static bool mouse_grabbed = false;

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
bool ClientInGame()
{
	return core.client.initialized && core.client.state == Client::Spawned;
}
bool ServerIsActive()
{
	return core.server.active;
}

void GetPlayerSpawnPoisiton(Entity* ent)
{
	Level* level = core.game.level;
	if (level->spawns.size() > 0) {
		ent->position = level->spawns[0].position;
		ent->rotation.y = level->spawns[0].angle;
	}
	else {
		ent->position = glm::vec3(0);
		ent->rotation = glm::vec3(0);
	}
}

void PlayerSpawn(Entity* ent)
{
	ASSERT(ent->type == Ent_Player);
	ent->model = FindOrLoadModel("CT.glb");
	if (ent->model) {
		int idle = ent->model->animations->FindClipFromName("act_idle");
		ent->animator.Init(ent->model);
		ent->animator.SetAnim(LOWERBODY_LAYER, idle);
	}
	GetPlayerSpawnPoisiton(ent);
	ent->ducking = false;
}

void DummySpawn(Entity* ent)
{

}

Entity* InitNewEnt(EntType type, int index)
{
	Entity* ent = &core.game.ents[index];
	core.game.spawnids[index]++;
	ASSERT(ent->type==Ent_Free);
	ent->type = type;
	ent->index = index;
	ent->ducking = false;
	ent->model = nullptr;
	ent->animator = Animator();
	ent->position = glm::vec3(0.f);
	ent->velocity = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->scale = glm::vec3(1.f);
	return ent;
}

void Game::SpawnNewClient(int client)
{
	Entity* ent = InitNewEnt(Ent_Player, client);
	PlayerSpawn(ent);
	core.game.num_ents++;
	printf("spawned client %d into game\n", client);
}
Entity* Game::LocalPlayer()
{
	ASSERT(ClientInGame());
	Entity* ent = GetByIndex(core.client.player_num);
	ASSERT(ent && ent->type == Ent_Player);
	return ent;
}
int Game::SpawnNewEntity(EntType type, vec3 pos, vec3 rot)
{
	if (type == Ent_Player)
		return -1;
	int slot = MAX_CLIENTS;
	for (; slot < MAX_GAME_ENTS; slot++) {
		if (ents[slot].type == Ent_Free)
			break;
	}
	if (slot == MAX_GAME_ENTS)
		return -1;
	Entity* ent = InitNewEnt(type, slot);
	core.game.num_ents++;
	printf("spawning ent in slot %d\n", slot);

	switch (type)
	{
	case Ent_Dummy:
		DummySpawn(ent);
		break;
	}
	return slot;
}
void Game::ClearAllEnts()
{
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		ents[i].type = Ent_Free;
		spawnids[i] = 0;
	}
	num_ents = 0;
}
bool Game::DoNewMap(const char* mapname)
{
	if (level) {
		ClearAllEnts();
		// free references in engine
		delete level;
	}
	level = LoadLevelFile(mapname);
	if (!level)
		return false;
	InitWorldCollision();
	return true;
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

void ViewMgr::Update()
{
	if (!ClientInGame())
		return;
	Core::InputState& input = core.input;
	if (update_camera) {
		fly_cam.UpdateFromInput(input.keyboard, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	}
	if (third_person) {
		Entity* player = core.game.LocalPlayer();
		fly_cam.position = player->position+vec3(0,0.5,0) - fly_cam.front * 3.f;
	}
	setup.view_mat = glm::lookAt(fly_cam.position, fly_cam.position + fly_cam.front, fly_cam.up);
	setup.viewproj = setup.proj_mat * setup.view_mat;
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

	SDL_GL_SetSwapInterval(1);
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
static int col_iters = 5;
static bool col_closest = true;
static bool new_physics = false;

static double TEMP_DT = 0.0;

static float friction = 6;
static float gravityamt = 2;
static float acceleration = 10;
static float minspeed = 1;
static float maxspeed = 30;

class PlayerPhysics
{
public:
	void Run(Entity* p, MoveCommand cmd, float dt);

private:
	void DoStandardPhysics();
	void ApplyFriction();
	void GroundMove();
	void AirMove();
	void FlyMove();
	void CheckJump();
	void CheckDuck();
	void CheckNans();

	float deltat;
	Entity* player;
	MoveCommand inp;
	vec2 inp_dir;
	float inp_len;

	vec3 look_front;
	vec3 look_side;
};

void PlayerPhysics::CheckNans()
{
	if (player->position.x != player->position.x || player->position.y != player->position.y ||
		player->position.z != player->position.z)
	{
		printf("origin nan found in PlayerPhysics\n");
		player->position = vec3(0);
	}
	if (player->velocity.x != player->velocity.x
		|| player->velocity.y != player->velocity.y || player->velocity.z != player->velocity.z)
	{
		printf("velocity nan found in PlayerPhysics\n");
		player->velocity = vec3(0);
	}
}

void PlayerPhysics::CheckJump()
{
	//if (player->onground && inp.buttonmask & B_JUMP)
	///	player->velocity.y += 5;
}
void PlayerPhysics::CheckDuck()
{
	if (inp.button_mask & CmdBtn_Duck) {
		player->ducking = true;
		return;
	}
	else if (!(inp.button_mask & CmdBtn_Duck) && player->ducking) {
		Capsule collider = CROUCH_COLLIDER;
		ColliderCastResult res;
		TraceCapsule(player->position, collider, &res, false);
		if (!res.found || (res.found && dot(res.surf_normal, vec3(0, 1, 0)) > 0))
			player->ducking = false;
	}
}

void PlayerPhysics::ApplyFriction()
{
	if (!player->on_ground)
		return;

	float speed = length(player->velocity);
	if (speed < 0.0001)
		return;

	float dropamt = friction * speed * deltat;

	float newspd = speed - dropamt;
	if (newspd < 0)
		newspd = 0;
	float factor = newspd / speed;

	player->velocity *= factor;
}

void PlayerPhysics::GroundMove()
{
	vec3 wishdir = (look_front * inp_dir.x + look_side * inp_dir.y);
	wishdir.y = 0;
	float wishspeed = inp_len * maxspeed;
	float addspeed = wishspeed - dot(player->velocity, wishdir);
	float accelspeed = acceleration * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	player->velocity += accelspeed * wishdir * deltat;

	float len = length(player->velocity);
	if (len > maxspeed)
		player->velocity = player->velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		player->velocity = vec3(0);

	CheckJump();
	CheckDuck();

	player->velocity += vec3(0, -1, 0) * gravityamt * deltat;

	vec3 delta = player->velocity * deltat;


	player->on_ground = false;
	Capsule cap = (player->ducking)?CROUCH_COLLIDER:DEFAULT_COLLIDER;
	vec3 position = player->position;
	vec3 step = delta / (float)col_iters;
	for (int i = 0; i < col_iters; i++)
	{
		position += step;
		ColliderCastResult trace;
		TraceCapsule(position, cap, &trace, col_closest);
		if (trace.found)
		{
			vec3 penetration_velocity = dot(player->velocity, trace.penetration_normal) * trace.penetration_normal;
			vec3 slide_velocity = player->velocity - penetration_velocity;
			position += trace.penetration_normal * trace.penetration_depth;////trace.0.001f + slide_velocity * dt;
			player->velocity = slide_velocity;

			if (trace.surf_normal.y > 0.2)
				player->on_ground = true;
		}
	}
	player->position = position;
}
static void UpdatePlayer(Entity* player, MoveCommand cmd, double dt);
void PlayerPhysics::Run(Entity* p, MoveCommand cmd, float dt)
{
	if (!p)
		return;

	deltat = dt;
	player = p;
	inp = cmd;// TheGame.lastinput;

	vec2 inputvec = vec2(inp.forward_move, inp.lateral_move);
	inp_len = length(inputvec);
	if (inp_len > 0.00001)
		inp_dir = inputvec / inp_len;
	if (inp_len > 1)
		inp_len = 1;

	// Store off last values for interpolation
	//p->lastorigin = p->origin;
	//p->lastangles = p->angles;

	//player->viewangles = inp.desired_view_angles;
	look_front = AnglesToVector(inp.view_angles.x, inp.view_angles.y);
	look_side = -cross(look_front, vec3(0, 1, 0));

	ApplyFriction();


	if (new_physics) {
		GroundMove();
	}
	else {
		// Old function
		UpdatePlayer(player, cmd, dt);
	}
	CheckNans();
}


static void UpdatePlayer(Entity* player, MoveCommand cmd, double dt)
{
	vec3 position = player->position;
	if(gravity)
		player->velocity.y -= 0.02f * dt;
	position += player->velocity;
	float move_speed = move_speed_player;

	vec3 front = core.view.fly_cam.front;
	vec3 up = vec3(0, 1, 0);
	vec3 right = cross(up, front);
	position += move_speed * front * cmd.forward_move;
	position += move_speed * right * cmd.lateral_move;
	position += move_speed * up * cmd.up_move;

	if(cmd.button_mask&CmdBtn_Misc1)
		move_speed += (move_speed * 0.5);
	else if(cmd.button_mask&CmdBtn_Misc2)
		move_speed -= (move_speed * 0.5);

	if (abs(move_speed) < 0.000001)
		move_speed = 0.0001;

	player->ducking = false;
	if (cmd.button_mask & CmdBtn_Duck)
		player->ducking = true;

	phys_debug.Begin();
	vec3 step = (position - player->position) / (float)col_iters;
	position = player->position;
	Capsule collider = (player->ducking) ? CROUCH_COLLIDER : DEFAULT_COLLIDER;
	for (int i = 0; i < col_iters; i++) {
		ColliderCastResult res;
		position += step;
		TraceCapsule(position, collider, &res,col_closest);
		if (res.found) {
			player->velocity = vec3(0);
			if(col_response)
				position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
			phys_debug.AddSphere(res.intersect_point, 0.1, 8, 6, COLOR_BLUE);
		}
	}
	for (int i = 0; i < 3; i++) {
		ColliderCastResult res;
		TraceCapsule(position, collider, &res, col_closest);
		if (res.found) {
			if (col_response)
				position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
			phys_debug.AddSphere(res.intersect_point, 0.1, 8, 6, COLOR_BLUE);
		}
	}

	collider.base += position;
	collider.tip += position;
	Bounds cap_b = CapsuleToAABB(collider);
	phys_debug.PushLineBox(cap_b.bmin, cap_b.bmax, COLOR_PINK);
	phys_debug.End();

	player->position = position;
	move_speed_player = move_speed;
}

void Game::ExecuteCommand(MoveCommand cmd, Entity* ent)
{
	PlayerPhysics physics;
	physics.Run(ent, cmd, TEMP_DT);
}

void CreateMoveCommand()
{
	if (mouse_grabbed) {
		float x_off = core.input.mouse_delta_x;
		float y_off = core.input.mouse_delta_y;
		const float sensitivity = 0.01;
		x_off *= sensitivity;
		y_off *= sensitivity;

		glm::vec3 view_angles = core.client.view_angles;
		view_angles.x -= y_off;	// pitch
		view_angles.y += x_off;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);

		core.client.view_angles = view_angles;
	}
	MoveCommand new_cmd{};
	new_cmd.view_angles = core.client.view_angles;
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

	new_cmd.tick = 0;	// TODO

	core.client.commands[core.client.GetOutSequence() % CLIENT_MOVE_HISTORY] = new_cmd;
}


void DrawTempLevel(mat4 viewproj)
{
	if (static_wrld.ID == 0)
		Shader::compile(&static_wrld, "AnimBasicV.txt", "AnimBasicF.txt","VERTEX_COLOR");
	static_wrld.use();
	static_wrld.set_mat4("ViewProj", viewproj);

	Level* level = core.game.level;
	for (int m = 0; m < level->render_data.instances.size(); m++) {
		Level::StaticInstance& sm = level->render_data.instances[m];
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

static void DrawPlayer(mat4 viewproj)
{
	Entity* player = core.game.LocalPlayer();
	vec3 origin = player->position;

	const Capsule& c = (player->ducking)? CROUCH_COLLIDER : DEFAULT_COLLIDER;
	float radius = c.radius;
	vec3 a, b;
	c.GetSphereCenters(a, b);

	MeshBuilder mb;
	mb.Begin();
	mb.AddSphere(origin + a, radius, 10, 7, COLOR_GREEN);
	mb.AddSphere(origin + b, radius, 10, 7, COLOR_GREEN);
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
	ViewSetup view = core.view.GetSceneView();
	//mat4 perspective = glm::perspective(fov, (float)vid_width / vid_height, 0.01f, 100.0f);
	//mat4 view = glm::lookAt(the_player.position - fly_cam.front * 2.f, the_player.position, vec3(0, 1, 0));
	glViewport(view.x, view.y, view.width, view.height);
	mat4 viewproj = view.viewproj;

	MeshBuilder mb;
	simple.use();
	simple.set_mat4("ViewProj", viewproj);
	simple.set_mat4("Model", mat4(1.f));
	DrawCollisionWorld();
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

	if (ClientInGame())
		Render(dt);

	SDL_GL_SwapWindow(core.window);
}

void MiscInit()
{
	core.game.DoNewMap("maze.glb");

	ASSERT(glCheckError() == 0);
}


void DoClientGameUpdate(double dt)
{
	
}

void DoGameUpdate(double dt)
{
	
}

void SpawnServer(const char* mapname)
{
	printf("Spawning server\n");
	bool good = core.game.DoNewMap(mapname);
	if (!good)
		return;
	core.server.active = true;
}

void CheckLocalServerRunning()
{
	if (core.client.state == Client::Disconnected && ServerIsActive()) {
		// connect to the local server
		IPAndPort serv_addr;
		serv_addr.SetIp(127, 0, 0, 1);
		serv_addr.port = SERVER_PORT;
		core.client.Connect(serv_addr);
	}
}

void ClientUpdate(double dt)
{
	Client* client = &core.client;
	if (!client->initialized)
		return;
	// make command
	if (client->state >= Client::Connected)
		CreateMoveCommand();
	CheckLocalServerRunning();
	client->TrySendingConnect();
	client->SendCommands();
	client->ReadPackets();
	DoClientGameUpdate(dt);
	if (core.game.num_ents > 0)
		client->state = Client::Spawned;
}

void ServerUpdate(double dt)
{
	Server* server = &core.server;
	if (!server->active)
		return;
	server->ReadPackets();
	DoGameUpdate(dt);
	server->SendSnapshots();
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
			int col_iter_old = col_iters;
			if (scancode == SDL_SCANCODE_LEFT)
				col_iters--;
			if (scancode == SDL_SCANCODE_RIGHT)
				col_iters++;
			if (col_iters <= 0) col_iters = 1;
			if (col_iters != col_iter_old)
				printf("col iters %d\n", col_iters);
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
			if (scancode == SDL_SCANCODE_1) {
				new_physics = !new_physics;
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

		}
	}
}

int main(int argc, char** argv)
{
	printf("Starting game\n");
	CreateWindow();
	NetworkInit();
	RenderInit();
	core.view.Init();
	core.server.Start();
	core.client.Start();
	SpawnServer(map_file);

	double delta_t = 0.1;
	double last = GetTime();
	for (;;)
	{
		double now = GetTime();
		delta_t = now - last;
		last = now;

		TEMP_DT = delta_t;
		HandleInput();
		ClientUpdate(delta_t);
		ServerUpdate(delta_t);
		if (core.client.initialized) {
			core.view.Update();
			DrawScreen(delta_t);
		}
	}
	
	return 0;
}