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
#include "ClientNServer.h"

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

struct Entity
{
	EntType type = EntType::INVALID;
	bool active = false;
	glm::vec3 position=glm::vec3(0);
	glm::vec3 rotation=glm::vec3(0);
	glm::vec3 scale = glm::vec3(1.f);
	const Model* model = nullptr;
	Capsule collider;
	
	glm::vec3 velocity=glm::vec3(0);
};

const int MAX_GAME_ENTS = 256;
class Game
{
public:
	Game() {
		memset(spawnids, 0, sizeof(spawnids));
	}
	int SpawnActor(EntType type);
	Entity* GetByIndex(int index) {
		if (index >= MAX_GAME_ENTS|| index < 0 || !ents[index].active)
			return nullptr;
		return &ents[index];
	}
	Entity* GetPlayer() { 
		Entity* e = GetByIndex(0);
		ASSERT(!e || e->type == EntType::PLAYER);
		return e;
	}

	Entity ents[MAX_GAME_ENTS];
	short spawnids[MAX_GAME_ENTS];
	int num_actors = 0;
	int first_free_network = 0;
	int first_free_local = 0;
	
	Level* level = nullptr;
};

int Game::SpawnActor(EntType type)
{
	// temp
	ents[num_actors].type = type;
	ents[num_actors++].active = true;
	return num_actors - 1;
}

class FlyCamera
{
public:
	vec3 position = vec3(0);
	vec3 front = vec3(1, 0, 0);
	vec3 up = vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	void UpdateFromInput(const bool keys[], int mouse_dx, int mouse_dy, int scroll);
	void UpdateVectors() {
		front = AnglesToVector(pitch, yaw);
	}
	mat4 GetViewMatrix() const {
		return glm::lookAt(position, position + front, up);
	}
};

struct ViewSetup
{
	vec3 vieworigin;
	vec3 viewfront;
	float viewfov;
	mat4 view_mat;
	mat4 proj_mat;
	mat4 viewproj;
	int x, y, width, height;
};

static bool update_camera = false;
class ViewMgr
{
public:
	void Init();
	void Update();
	const ViewSetup& GetSceneView() {
		return setup;
	}

	bool third_person = true;
	bool debug_fly = false;

	float z_near = 0.01f;
	float z_far = 100.f;
	float fov = glm::radians(70.f);

	FlyCamera fly_cam;
	ViewSetup setup;
};


const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;
class Core
{
public:
	SDL_Window* window = nullptr;
	SDL_GLContext context = nullptr;
	int vid_width = DEFAULT_WIDTH;
	int vid_height = DEFAULT_HEIGHT;

	struct InputState
	{
		bool keyboard[SDL_NUM_SCANCODES];
		int mouse_delta_x = 0;
		int mouse_delta_y = 0;
		int scroll_delta = 0;
	};
	InputState input;
	Game game;
	ViewMgr view;
	Client client;
	Server server;
};
Core core;

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

void Fatal(const char* msg)
{
	Cleanup();
	exit(-1);
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
	Core::InputState& input = core.input;
	if (update_camera) {
		fly_cam.UpdateFromInput(input.keyboard, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	}
	if (third_person) {
		Entity* player = core.game.GetPlayer();
		fly_cam.position = player->position - fly_cam.front * 3.f;
	}
	setup.view_mat = glm::lookAt(fly_cam.position, fly_cam.position + fly_cam.front, fly_cam.up);
	setup.viewproj = setup.proj_mat * setup.view_mat;
}


void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(1);
}

bool CreateWindow()
{
	if (core.window)
		return true;

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		SDLError("SDL init failed");
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	core.window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
		core.vid_width, core.vid_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (core.window == nullptr) {
		SDLError("SDL failed to create window");
		return false;
	}

	core.context = SDL_GL_CreateContext(core.window);
	printf("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(1);

	return true;
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
Level* TEMP_LEVEL;
Model* gun;

const Capsule DEFAULT_COLLIDER = {
	0.2f, glm::vec3(0,0,0),glm::vec3(0,1.3,0.0)
};
const Capsule CROUCH_COLLIDER = {
	0.2f, glm::vec3(0,0,0),glm::vec3(0,0.75,0.0)
};
const float DEFAULT_MOVESPEED = 0.1f;

double GetTime()
{
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

static float move_speed_player = 0.1f;

static void UpdatePlayer(const Core::InputState& input, double dt)
{
	const bool* keys = input.keyboard;
	const int scroll = input.scroll_delta;
	
	Entity* player = core.game.GetPlayer();
	vec3 position = player->position;
	float move_speed = move_speed_player;

	vec3 front = core.view.fly_cam.front;
	vec3 up = vec3(0, 1, 0);
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

	if (keys[SDL_SCANCODE_LSHIFT])
		player->collider = CROUCH_COLLIDER;
	else
		player->collider = DEFAULT_COLLIDER;

	position.y -= 0.9f * dt;

	Capsule collider = player->collider;
	for (int i = 0; i < 4; i++) {
		ColliderCastResult res;
		TraceCapsule(position, collider, &res);
		if (res.found) {
			position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
		}
	}

	player->position = position;
	move_speed_player = move_speed;
}

void Update(double dt)
{
	UpdatePlayer(core.input, dt);
	core.view.Update();
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
	Entity* player = core.game.GetPlayer();
	vec3 origin = player->position;
	float radius = player->collider.radius;
	vec3 a, b;
	player->collider.GetSphereCenters(a, b);

	MeshBuilder mb;
	mb.Begin();
	mb.AddSphere(origin + a, player->collider.radius, 10, 7, COLOR_GREEN);
	mb.AddSphere(origin + b, player->collider.radius, 10, 7, COLOR_GREEN);
	mb.End();
	mb.Draw(GL_LINES);
	mb.Free();
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
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);


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
		//glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}
	glCheckError();

	DrawTempLevel(viewproj);
	SDL_GL_SwapWindow(core.window);

}

void MiscInit()
{
	core.game.level = LoadLevelFile("world0.glb");
	TEMP_LEVEL = core.game.level;

	int p_index = core.game.SpawnActor(EntType::PLAYER);
	Entity* player = core.game.GetPlayer();
	player->collider = DEFAULT_COLLIDER;

	if (!core.game.level->spawns.empty()) {
		Level::PlayerSpawn& spawn = core.game.level->spawns[0];
		player->position = spawn.position;
		core.view.fly_cam.yaw = spawn.angle;
	}

	ASSERT(mytexture != nullptr);
	ASSERT(glCheckError() == 0);

	animator.Init(m);
	animator.SetAnim(0, 2);
	animator.GetLayer(0).active = true;

	int bone = m->BoneForName("upper_arm.R");
	int anim = m->animations->FindClipFromName("act_run");
	if (anim != -1)
		animator.SetAnim(0, anim);
}

int Run()
{
	double delta_t = 0.1;
	double last = GetTime();
	for (;;)
	{
		double now = GetTime();
		delta_t = now - last;
		last = now;

		Core::InputState& input = core.input;
		input.mouse_delta_x = input.mouse_delta_y = input.scroll_delta = 0;
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				Quit();
				return 1;
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				input.keyboard[event.key.keysym.scancode] = event.key.type == SDL_KEYDOWN;
				break;
			case SDL_MOUSEMOTION:
				input.mouse_delta_x += event.motion.xrel;
				input.mouse_delta_y += event.motion.yrel;
				break;
			case SDL_MOUSEWHEEL:
				input.scroll_delta += event.wheel.y;
			case SDL_MOUSEBUTTONDOWN:
				SDL_SetRelativeMouseMode(SDL_TRUE);
				update_camera = true;
				break;
			case SDL_MOUSEBUTTONUP:
				SDL_SetRelativeMouseMode(SDL_FALSE);
				update_camera = false;
				break;
		
			}
		}
		// Service net
		core.client.TrySendingConnect();
		core.server.ReadMessages();

		Update(delta_t);
		Render(delta_t);
	}
}

int main(int argc, char** argv)
{
	printf("Starting...\n");
	CreateWindow();
	core.view.Init();
	RenderInit();
	NetworkInit();
	core.server.Start();
	core.client.Start();
	IPAndPort serv_addr;
	serv_addr.SetIp(127, 0, 0, 1);
	serv_addr.port = SERVER_PORT;
	core.client.Connect(serv_addr);
	MiscInit();

	Run();
	
	Quit();
	
	return 0;
}