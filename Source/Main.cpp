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
const char* map_file = "test_level2.glb";
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

	void(*trace_func)(int) = nullptr;
	void(*impact_func)(int, int) = nullptr;

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
	if (player->on_ground && inp.button_mask & CmdBtn_Jump) {
		printf("jump\n");
		player->velocity.y += jumpimpulse;
		player->on_ground = false;
	}
}
void PlayerPhysics::CheckDuck()
{
	if (inp.button_mask & CmdBtn_Duck) {
		if (player->on_ground) {
			player->ducking = true;
		}
		else if(!player->on_ground&&!player->ducking){
			const Capsule& st = DEFAULT_COLLIDER;
			const Capsule& cr = CROUCH_COLLIDER;
			// Move legs of player up
			player->position.y += st.tip.y - cr.tip.y;
			player->ducking = true;
		}
	}
	else if (!(inp.button_mask & CmdBtn_Duck) && player->ducking) {
		int steps = 2;
		float step = 0.f;
		float sphere_radius = 0.f;
		vec3 offset = vec3(0.f);
		vec3 a, b, c, d;
		DEFAULT_COLLIDER.GetSphereCenters(a, b);
		CROUCH_COLLIDER.GetSphereCenters(c, d);
		float len = b.y - d.y;
		sphere_radius = CROUCH_COLLIDER.radius;
		if (player->on_ground) {
			step = len / (float)steps;
			offset = d;
		}
		else {
			steps = 3;
			// testing downwards to ground
			step = -(len+0.1) / (float)steps;
			offset = c;
		}
		int i = 0;
		for (; i < steps; i++) {
			ColliderCastResult res;
			vec3 where = player->position + offset + vec3(0, (i + 1) * step, 0);
			TraceSphere(level,where, sphere_radius, &res, true, false);
			if (res.found) {
				phys_debug.AddSphere(where, sphere_radius, 10, 8, COLOR_RED);
				break;
			}
		}
		if (i == steps) {
			player->ducking = false;
			if (!player->on_ground) {
				player->position.y -= len;
			}
		}
	}
}

void PlayerPhysics::ApplyFriction(float friction_val)
{
	float speed = length(player->velocity);
	if (speed < 0.0001)
		return;

	float dropamt = friction_val * speed * deltat;

	float newspd = speed - dropamt;
	if (newspd < 0)
		newspd = 0;
	float factor = newspd / speed;

	player->velocity.x *= factor;
	player->velocity.z *= factor;

}
void PlayerPhysics::MoveAndSlide(vec3 delta)
{
	Capsule cap = (player->ducking) ? CROUCH_COLLIDER : DEFAULT_COLLIDER;
	vec3 position = player->position;
	vec3 step = delta / (float)col_iters;
	for (int i = 0; i < col_iters; i++)
	{
		position += step;
		ColliderCastResult trace;
		TraceCapsule(level,position, cap, &trace, col_closest);
		if (trace.found)
		{
			vec3 penetration_velocity = dot(player->velocity, trace.penetration_normal) * trace.penetration_normal;
			vec3 slide_velocity = player->velocity - penetration_velocity;
			position += trace.penetration_normal * trace.penetration_depth;////trace.0.001f + slide_velocity * dt;
			player->velocity = slide_velocity;
		}
	}
	// Gets player out of surfaces
	for (int i = 0; i < 2; i++) {
		ColliderCastResult res;
		TraceCapsule(level,position, cap, &res, col_closest);
		if (res.found) {
			position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
		}
	}
	player->position = position;
}

void PlayerPhysics::CheckGroundState()
{
	if (player->velocity.y > 2.f) {
		player->on_ground = false;
		return;
	}
	ColliderCastResult result;
	vec3 where = player->position - vec3(0, 0.005-DEFAULT_COLLIDER.radius, 0);
	float radius = DEFAULT_COLLIDER.radius;
	TraceSphere(level,where,radius, &result,true,true);
	if (!result.found)
		player->on_ground = false;
	else if (result.surf_normal.y < 0.3)
		player->on_ground = false;
	else {
		player->on_ground = true;
		//phys_debug.AddSphere(where, radius, 8, 6, COLOR_BLUE);
	}
}

static float max_ground_speed = 10;
static float max_air_speed = 2;

void PlayerPhysics::GroundMove()
{
	float acceleation_val = (player->on_ground) ? ground_accel : air_accel;
	float maxspeed_val = (player->on_ground) ? max_ground_speed : max_air_speed;

	vec3 wishdir = (look_front * inp_dir.x + look_side * inp_dir.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(player->velocity.x, 0, player->velocity.z);

	float wishspeed = inp_len * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed*deltat;
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	player->velocity = vec3(xz_velocity.x, player->velocity.y, xz_velocity.z);
	

	CheckJump();
	CheckDuck();
	if (!player->on_ground)
		player->velocity.y -= gravityamt * deltat;

	vec3 delta = player->velocity * deltat;

	MoveAndSlide(delta);
}
void PlayerPhysics::AirMove()
{

}
static void UpdatePlayer(Entity* player, MoveCommand cmd, double dt);
void PlayerPhysics::Run(const Level* lvl, Entity* p, MoveCommand cmd, float dt)
{
	if (!p)
		return;
	level = lvl;
	deltat = dt;
	player = p;
	inp = cmd;

	vec2 inputvec = vec2(inp.forward_move, inp.lateral_move);
	inp_len = length(inputvec);
	if (inp_len > 0.00001)
		inp_dir = inputvec / inp_len;
	if (inp_len > 1)
		inp_len = 1;

	phys_debug.PushLine(p->position, p->position + glm::vec3(inputvec.x, 0.2f,inputvec.y), COLOR_WHITE);

	// Store off last values for interpolation
	//p->lastorigin = p->origin;
	//p->lastangles = p->angles;

	//player->viewangles = inp.desired_view_angles;
	look_front = AnglesToVector(inp.view_angles.x, inp.view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	look_side = -cross(look_front, vec3(0, 1, 0));



	if (new_physics) {
		float fric_val = (player->on_ground) ? ground_friction : air_friction;
		ApplyFriction(fric_val);
		CheckGroundState();	// check ground after applying friction, like quake
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
	player->velocity = vec3(0.f);
	vec3 position = player->position;
	
	position += player->velocity;
	float move_speed = move_speed_player;

	vec3 front = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);
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

	vec3 step = (position - player->position) / (float)col_iters;
	position = player->position;
	Capsule collider = (player->ducking) ? CROUCH_COLLIDER : DEFAULT_COLLIDER;
	for (int i = 0; i < col_iters; i++) {
		ColliderCastResult res;
		position += step;
		TraceCapsule(server.sv_game.level,position, collider, &res,col_closest);
		if (res.found) {
			player->velocity = vec3(0);
			if(col_response)
				position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
			//phys_debug.AddSphere(res.intersect_point, 0.1, 8, 6, COLOR_BLUE);
		}
	}
	for (int i = 0; i < 3; i++) {
		ColliderCastResult res;
		TraceCapsule(server.sv_game.level,position, collider, &res, col_closest);
		if (res.found) {
			if (col_response)
				position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
			//phys_debug.AddSphere(res.intersect_point, 0.1, 8, 6, COLOR_BLUE);
		}
	}

	collider.base += position;
	collider.tip += position;
	Bounds cap_b = CapsuleToAABB(collider);
	phys_debug.PushLineBox(cap_b.bmin, cap_b.bmax, COLOR_PINK);

	player->position = position;
	move_speed_player = move_speed;
}

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
#include "Movement.h"


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
	//
	ent->ducking = state->ducking;
	ent->on_ground = state->on_ground;
	ent->velocity = state->velocity;
}


void Game::ExecutePlayerMove(Entity* ent, MoveCommand cmd)
{
	if (paused)
		return;
	phys_debug.Begin();
	PlayerPhysics physics;
	physics.Run(level,ent, cmd, core.tick_interval);
	phys_debug.End();

	//PlayerMovement move;
	//move.cmd = cmd;
	//move.deltat = core.tick_interval;
	//move.phys_debug = &phys_debug;
	//move.trace_callback = Server_TraceCallback;
	//EntToPlayerState(&move.in_state, ent);
	//move.Run();
	//PlayerStateToEnt(ent, move.GetOutputState());

	

	DoGunLogic(ent, cmd);
	
}

void Client::CreateMoveCmd()
{
	if (mouse_grabbed) {
		float x_off = core.input.mouse_delta_x;
		float y_off = core.input.mouse_delta_y;
		const float sensitivity = 0.01;
		x_off *= sensitivity;
		y_off *= sensitivity;

		glm::vec3 view_angles = this->view_angles;
		view_angles.x -= y_off;	// pitch
		view_angles.y += x_off;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
		this->view_angles = view_angles;
	}
	MoveCommand new_cmd{};
	new_cmd.view_angles = view_angles;
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

	new_cmd.tick = tick;

	*GetCommand(GetCurrentSequence()) = new_cmd;
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
	EntityState interp = client.GetLocalPlayer()->state;
	//interp.position = GetLocalPlayerInterpedOrigin();
	
	DrawState(&interp, COLOR_GREEN, mb);

	for (int i = 0; i < client.cl_game.entities.size(); i++) {
		auto& ent = client.cl_game.entities[i];
		if (ent.state.type != Ent_Free && i != client.GetPlayerNum()) {
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
	mb.Free();
	DrawPlayer(viewproj);

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
	DrawTempLevel(viewproj);
}

void DrawScreen(double dt)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	if (client.IsInGame())
		Render(dt);

	SDL_GL_SwapWindow(core.window);
}


void DoClientGameUpdate(double dt)
{
	
}

void DoGameUpdate(double dt)
{
	
}

void Client_TraceCallback(ColliderCastResult* out, PhysContainer obj, bool closest, bool double_sided)
{
	TraceAgainstLevel(client.cl_game.level, out, obj, closest, double_sided);
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

	physics.Run(client.cl_game.level, &ent, *cmd, core.tick_interval);

	out->estate.ducking = ent.ducking;
	out->estate.position = ent.position;
	out->estate.angles = ent.rotation;

	out->pstate.on_ground = ent.on_ground;
	out->pstate.velocity = ent.velocity;

	//PlayerMovement move;
	//move.cmd = *cmd;
	//move.deltat = core.tick_interval;
	//move.phys_debug = &phys_debug;
	//move.in_state = in->pstate;
	//move.trace_callback = Client_TraceCallback;
	//move.Run();
	//
	//out->pstate = *move.GetOutputState();
}

void DoClientPrediction()
{
	if (client.GetConState() != Spawned)
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
	
	ClientEntity* ent = client.GetLocalPlayer();
	ent->state = pred_state.estate;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).tick = client.tick;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).origin = ent->state.position;
	//ent->current_hist_index++;
	//ent->current_hist_index %= ent->transform_hist.size();
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

void Client::CheckLocalServerIsRunning()
{
	if (GetConState() == Disconnected && server.IsActive()) {
		// connect to the local server
		IPAndPort serv_addr;
		serv_addr.SetIp(127, 0, 0, 1);
		serv_addr.port = SERVER_PORT;
		server_mgr.Connect(serv_addr);
	}
}

void Client::RunPrediction()
{
	if (GetConState() != Spawned)
		return;
	// predict commands from outgoing ack'ed to current outgoing
	// TODO: dont repeat commands unless a new snapshot arrived
	int start = server_mgr.OutSequenceAk();	// start at the new cmd
	int end = server_mgr.OutSequence();
	int commands_to_run = end - start;
	if (commands_to_run > CLIENT_MOVE_HISTORY)	// overflow
		return;
	// restore state to last authoritative snapshot 
	int incoming_seq = server_mgr.InSequence();
	Snapshot* last_auth_state = &cl_game.snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);
	PredictionState pred_state;
	pred_state.estate = last_auth_state->entities[GetPlayerNum()];
	pred_state.pstate = last_auth_state->pstate;
	PredictionState next_state;
	for (int i = start + 1; i < end; i++) {
		MoveCommand* cmd = GetCommand(i);
		RunClientPhysics(&pred_state, &next_state, cmd);
		pred_state = next_state;
	}

	ClientEntity* ent = GetLocalPlayer();
	ent->state = pred_state.estate;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).tick = client.tick;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).origin = ent->state.position;
	//ent->current_hist_index++;
	//ent->current_hist_index %= ent->transform_hist.size();
}

void Client::FixedUpdateInput(double dt)
{
	if (!initialized)
		return;
	if (GetConState() >= Connected) {
		CreateMoveCmd();
	}
	server_mgr.SendMovesAndMessages();
	CheckLocalServerIsRunning();
	server_mgr.TrySendingConnect();
}
void Client::FixedUpdateRead(double dt)
{
	if (!initialized)
		return;
	if (GetConState() == Spawned)
		client.tick += 1;
	server_mgr.ReadPackets();
	RunPrediction();
}
void Client::PreRenderUpdate(double frametime)
{
	view_mgr.Update();
	DoClientGameUpdate(frametime);
}

#if 0
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
#endif

void Server::FixedUpdate(double dt)
{
	if (!IsActive())
		return;
	client_mgr.ReadPackets();
	DoGameUpdate(dt);
	client_mgr.SendSnapshots();
	tick += 1;
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
	if (argc == 2 && strcmp(argv[1], "-client") == 0)
		run_client_only = true;
	CreateWindow();
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