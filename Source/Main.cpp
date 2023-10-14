#include <SDL2/SDL.h>
#include "glad/glad.h"
#include "stb_image.h"
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
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
#include "Config.h"
#include "Media.h"

MeshBuilder phys_debug;
Core core;
ConfigMgr cfg;
Media media;

int* ConfigMgr::MakeI(const char* name, int default_val)
{
	GlobalVar* gv = FindInList(name);
	if (gv) {
		if (gv->type != GlobalVar::Int)
			Fatalf("ConfigMgr::get called with mismatched types: %s\n", name);
		return &gv->ival;
	}
	GlobalVar* v = InitNewVar(name);
	v->type = GlobalVar::Int;
	v->ival = default_val;
	return &v->ival;
}
float* ConfigMgr::MakeF(const char* name, float default_val)
{
	GlobalVar* gv = FindInList(name);
	if (gv) {
		if (gv->type != GlobalVar::Float)
			Fatalf("ConfigMgr::get called with mismatched types: %s\n", name);
		return &gv->fval;
	}
	GlobalVar* v = InitNewVar(name);
	v->type = GlobalVar::Float;
	v->fval = default_val;
	return &v->fval;
}

GlobalVar* ConfigMgr::InitNewVar(const char* name)
{
	GlobalVar* v = new GlobalVar;
	int namelen = strlen(name);
	if (namelen >= GlobalVar::NAME_LEN) {
		Fatalf("configmgr: name too long, %s\n", name);
	}
	memcpy(v->name, name, namelen);
	v->name[namelen] = '\0';
	v->next = nullptr;
	if (tail) {
		tail->next = v;
		tail = v;
	}
	else {
		head = tail = v;
	}
	return v;
}

int ConfigMgr::GetI(const char* name)
{
	auto gv = FindInList(name);
	ASSERT(gv);
	ASSERT(gv->type == GlobalVar::Int);
	return gv->ival;
}
float ConfigMgr::GetF(const char* name)
{
	auto gv = FindInList(name);
	ASSERT(gv);
	ASSERT(gv->type == GlobalVar::Float);
	return gv->fval;
}

void ConfigMgr::SetF(const char* name, float f)
{
	float* p = MakeF(name, f);
	*p = f;
}
void ConfigMgr::SetI(const char* name, int i)
{
	int* p = MakeI(name, i);
	*p = i;
}

void ConfigMgr::LoadFromDisk(const char* filepath)
{
	std::ifstream infile(filepath);
	if (!infile) {
		printf("No config file\n");
	}
	std::string line;
	std::string name;
	std::string val;
	while (std::getline(infile, line)) {
		if (line.empty())
			continue;
		if (line.at(0) == '#')
			continue;

		std::stringstream ss;
		ss << line;
		ss >> name >> val;

		try {
			if (val.find('.') != std::string::npos) {
				SetF(name.c_str(), std::stof(val));
			}
			else {
				SetI(name.c_str(), std::stoi(val));
			}
			printf("Found cfg var: %s : %s\n", name.c_str(), val.c_str());
		}
		catch (std::invalid_argument) {
			printf("Bad cfg value: %s\n", line.c_str());
		}
	}
}

GlobalVar* ConfigMgr::FindInList(const char* name)
{
	GlobalVar* p = head;
	while (p) {
		if (strcmp(p->name, name) == 0) {
			return p;
		}
		p = p->next;
	}
	return nullptr;
}

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
	//SDL_Quit();
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

void ClientGame::UpdateCamera()
{
	if (!client.IsInGame())
		return;
	ViewSetup setup;

	// decay view_recoil
	view_recoil.x = view_recoil.x * 0.9f;

	vec3 true_viewangles = client.view_angles + view_recoil;

	if(view_recoil.y != 0)
		printf("view_recoil: %f\n", view_recoil.y);

	vec3 true_front = AnglesToVector(true_viewangles.x, true_viewangles.y);

	setup.height = core.vid_height;
	setup.width = core.vid_width;
	setup.viewfov = fov;
	setup.x = setup.y = 0;
	setup.proj_mat = glm::perspective(setup.viewfov, (float)setup.width / setup.height, z_near, z_far);
	setup.near = z_near;
	setup.far = z_far;
	Core::InputState& input = core.input;

	if (update_camera) {
		fly_cam.UpdateFromInput(input.keyboard, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	}

	if (third_person) {
		ClientEntity* player = client.GetLocalPlayer();
		vec3 front = AnglesToVector(client.view_angles.x, client.view_angles.y);
		//fly_cam.position = GetLocalPlayerInterpedOrigin()+vec3(0,0.5,0) - front * 3.f;
		fly_cam.position = player->interpstate.position + vec3(0, STANDING_EYE_OFFSET, 0) - front * 4.5f;
		setup.view_mat = glm::lookAt(fly_cam.position, fly_cam.position + front, fly_cam.up);
		setup.viewproj = setup.proj_mat * setup.view_mat;
		setup.viewfront = front;
		setup.vieworigin = fly_cam.position;
	}
	else
	{
		ClientEntity* player = client.GetLocalPlayer();
		EntityState* pstate = &player->interpstate;
		float view_height = (pstate->ducking) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
		vec3 cam_position = pstate->position + vec3(0, view_height, 0);
		setup.view_mat = glm::lookAt(cam_position, cam_position + true_front, vec3(0, 1, 0));
		setup.viewproj = setup.proj_mat * setup.view_mat;
		setup.vieworigin = cam_position;
		setup.viewfront = true_front;
	}

	last_view = setup;
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

struct RenderAttachments
{
	uint32_t scene_color;
	uint32_t scene_depthstencil;
	uint32_t ssao_color;
};

struct RenderFBOs
{
	uint32_t scene;
	uint32_t ssao;
};


class Renderer
{
public:
	struct DrawEntry {
		const Model* m;
		const Animator* a;
		mat4 matrix;
	};

	void Init();
	void FrameDraw();

	void DrawTexturedQuad();
	void DrawText();

	void DrawModel(const Model* m, mat4 transform, const Animator* a = nullptr);
	void AddPlayerDebugCapsule(const EntityState* es, MeshBuilder* mb, Color32 color);

	// shaders
	Shader simple;
	Shader textured;
	Shader animated;
	Shader static_wrld;
	Shader basic_mod;
	Shader particle_basic;

	// std textures
	uint32_t white_texture;
	uint32_t black_texture;

	RenderFBOs fbo;
	RenderAttachments textures;

	// config vars
	int* cfg_draw_collision_tris;
	int* cfg_draw_sv_colliders;
	int* cfg_draw_viewmodel;
private:
	void InitGlState();
	void InitFramebuffers();

	void DrawEnts();
	void DrawLevel();
	void DrawPlayerViewmodel();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void BindTexture(int bind, int id);
	void SetStdShaderConstants(Shader* s);

	int cur_w = 0;
	int cur_h = 0;
	int cur_shader = -1;
	ViewSetup vs;

	int cur_img_1 = -1;

	ClientGame* cgame = nullptr;

	MeshBuilder shadowverts;
};

void Renderer::InitGlState()
{
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);
	glEnable(GL_MULTISAMPLE);
	glDepthFunc(GL_LEQUAL);
}

Renderer rndr;

void Renderer::AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width)
{
	MbVertex corners[4];

	glm::vec3 side = (glm::abs(normal.x)<0.999)?cross(normal, vec3(1, 0, 0)): cross(normal,vec3(0,1,0));
	side = glm::normalize(side);
	glm::vec3 side2 = cross(side, normal);

	float halfwidth = width / 2.f;

	corners[0].position = org + side * halfwidth + side2 * halfwidth;
	corners[1].position = org - side * halfwidth + side2 * halfwidth;
	corners[2].position = org - side * halfwidth - side2 * halfwidth;
	corners[3].position = org + side * halfwidth - side2 * halfwidth;
	corners[0].uv = glm::vec2(0);
	corners[1].uv = glm::vec2(0,1);
	corners[2].uv = glm::vec2(1,1);
	corners[3].uv = glm::vec2(1,0);
	int base = shadowverts.GetBaseVertex();
	for (int i = 0; i < 4; i++) {
		shadowverts.AddVertex(corners[i]);
	}
	shadowverts.AddQuad(base, base + 1, base + 2, base + 3);
}

void Renderer::BindTexture(int bind, int id)
{
	if (id != cur_img_1) {
		glActiveTexture(GL_TEXTURE0 + bind);
		glBindTexture(GL_TEXTURE_2D, id);
		cur_img_1 = id;
	}
}

void Renderer::SetStdShaderConstants(Shader* s)
{
	s->set_mat4("ViewProj", vs.viewproj);
	
	// fog vars
	s->set_float("near", vs.near);
	s->set_float("far", vs.far);
	s->set_float("fog_max_density", 1.0);
	s->set_vec3("fog_color", vec3(0.7));
	s->set_float("fog_start", 10.f);
	s->set_float("fog_end", 30.f);

	s->set_vec3("view_front", vs.viewfront);
	s->set_vec3("light_dir", glm::normalize(-vec3(1)));

}

void Renderer::Init()
{
	InitGlState();
	Shader::compile(&simple, "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&textured, "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&animated, "AnimBasicV.txt", "AnimBasicF.txt", "ANIMATED");
	Shader::compile(&basic_mod, "AnimBasicV.txt", "AnimBasicF.txt");
	Shader::compile(&static_wrld, "AnimBasicV.txt", "AnimBasicF.txt", "VERTEX_COLOR");
	Shader::compile(&particle_basic, "MbTexturedV.txt", "MbTexturedF.txt", "PARTICLE_SHADER");



	const uint8_t wdata[] = { 0xff,0xff,0xff };
	const uint8_t bdata[] = { 0x0,0x0,0x0 };
	glGenTextures(1, &white_texture);
	glBindTexture(GL_TEXTURE_2D, white_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, wdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	glGenTextures(1, &black_texture);
	glBindTexture(GL_TEXTURE_2D, black_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, bdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	cfg_draw_collision_tris = cfg.MakeI("draw_collision_tris", 0);
	cfg_draw_sv_colliders = cfg.MakeI("draw_sv_colliders", 0);
	cfg_draw_viewmodel = cfg.MakeI("draw_viewmodel", 1);

	fbo.scene = fbo.ssao = 0;
	textures.scene_color = textures.scene_depthstencil = textures.ssao_color = 0;
	InitFramebuffers();

	cgame = &client.cl_game;
}

void Renderer::InitFramebuffers()
{
	const int s_w = core.vid_width;
	const int s_h = core.vid_height;

	glDeleteTextures(1, &textures.scene_color);
	glGenTextures(1, &textures.scene_color);
	glBindTexture(GL_TEXTURE_2D, textures.scene_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_w, s_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheckError();

	glDeleteTextures(1, &textures.scene_depthstencil);
	glGenTextures(1, &textures.scene_depthstencil);
	glBindTexture(GL_TEXTURE_2D, textures.scene_depthstencil);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, s_w, s_h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheckError();

	glDeleteFramebuffers(1, &fbo.scene);
	glGenFramebuffers(1, &fbo.scene);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures.scene_color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textures.scene_depthstencil, 0);
	glCheckError();

	glDeleteTextures(1, &textures.ssao_color);
	glGenTextures(1, &textures.ssao_color);
	glBindTexture(GL_TEXTURE_2D, textures.ssao_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, s_w, s_h, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCheckError();

	glDeleteFramebuffers(1, &fbo.ssao);
	glGenFramebuffers(1, &fbo.ssao);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.ssao);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures.ssao_color, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glCheckError();

	cur_w = s_w;
	cur_h = s_h;
}


void Renderer::FrameDraw()
{
	cur_shader = 0;
	cur_img_1 = 0;
	if (cur_w != core.vid_width || cur_h != core.vid_height)
		InitFramebuffers();

	vs = cgame->GetSceneView();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	glViewport(vs.x, vs.y, vs.width, vs.height);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	DrawEnts();
	DrawLevel();
	DrawEntBlobShadows();

	int x = vs.width;
	int y = vs.height;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.scene);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, x, y, 0, 0, x, y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBlitFramebuffer(0, 0, x, y, 0, 0, x, y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	MeshBuilder mb;
	mb.Begin();
	if (IsServerActive() && *cfg_draw_sv_colliders == 1) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			// FIXME: leaking server code into client code
			if (game.ents[i].type == Ent_Player) {
				EntityState es = game.ents[i].ToEntState();
				AddPlayerDebugCapsule(&es, &mb, COLOR_CYAN);
			}
		}
	}

	mb.End();
	simple.use();
	simple.set_mat4("ViewProj", vs.viewproj);
	simple.set_mat4("Model", mat4(1.f));

	if(*cfg_draw_collision_tris)
		DrawCollisionWorld(cgame->level);

	mb.Draw(GL_LINES);
	game.rays.End();
	game.rays.Draw(GL_LINES);
	if (IsServerActive()) {
		phys_debug.End();
		phys_debug.Draw(GL_LINES);
	}
	mb.Free();

	glCheckError();
	glClear(GL_DEPTH_BUFFER_BIT);

	if(cgame->ShouldDrawViewModel() && *cfg_draw_viewmodel)
		DrawPlayerViewmodel();
}

void Renderer::DrawEntBlobShadows()
{
	shadowverts.Begin();

	for (int i = 0; i < cgame->entities.size(); i++)
	{
		ClientEntity* ce = &cgame->entities[i];
		if (!ce->active) continue;
		EntityState* s = &ce->interpstate;
		
		RayHit rh;
		Ray r;
		r.pos = s->position + glm::vec3(0,0.1f,0);
		r.dir = glm::vec3(0, -1, 0);
		cgame->phys.TraceRay(r, &rh, i, Pf_World);

		if (rh.dist < 0)
			continue;

		AddBlobShadow(rh.pos + vec3(0,0.05,0), rh.normal, CHAR_HITBOX_RADIUS * 2.5f);
	}
	glCheckError();

	shadowverts.End();
	glCheckError();

	particle_basic.use();
	particle_basic.set_mat4("ViewProj", vs.viewproj);
	particle_basic.set_mat4("Model", mat4(1.0));
	particle_basic.set_vec4("tint_color", vec4(0,0,0,1));
	glCheckError();

	BindTexture(0, media.blobshadow->gl_id);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shadowverts.Draw(GL_TRIANGLES);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);

	cur_shader = -1;
	glCheckError();

}

void Renderer::DrawModel(const Model* m, mat4 transform, const Animator* a)
{
	ASSERT(m);
	const bool isanimated = a != nullptr;
	Shader* s;
	if (isanimated)
		s = &animated;
	else
		s = &basic_mod;

	if (s->ID != cur_shader) {
		s->use();
		SetStdShaderConstants(s);
		cur_shader = s->ID;
	}
	glCheckError();
	s->set_mat4("Model", transform);
	s->set_mat4("InverseModel", glm::inverse(transform));

	if (isanimated) {
		const std::vector<mat4>& bones = a->GetBones();
		const uint32_t bone_matrix_loc = glGetUniformLocation(s->ID, "BoneTransform[0]");
		for (int j = 0; j < bones.size(); j++)
			glUniformMatrix4fv(bone_matrix_loc + j, 1, GL_FALSE, glm::value_ptr(bones[j]));
		glCheckError();
	}

	for (int i = 0; i < m->parts.size(); i++)
	{
		const MeshPart* part = &m->parts[i];

		if (part->material_idx==-1) {
			BindTexture(0, white_texture);
		}
		else {
			const MeshMaterial& mm = m->materials.at(part->material_idx);
			if (mm.t1)
				BindTexture(0, mm.t1->gl_id);
			else
				BindTexture(0, white_texture);
		}

		glBindVertexArray(part->vao);
		glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}

}

void Renderer::DrawEnts()
{
	for (int i = 0; i < cgame->entities.size(); i++) {
		auto& ent = cgame->entities[i];
		if (!ent.active)
			continue;
		if (!ent.model)
			continue;

		if (i == client.GetPlayerNum() && !cgame->third_person)
			continue;

		EntityState* cur = &ent.interpstate;

		mat4 model = glm::translate(mat4(1), cur->position);
		model = model * glm::eulerAngleXYZ(cur->angles.x, cur->angles.y, cur->angles.z);
		model = glm::scale(model, vec3(1.f));

		const Animator* a = (ent.model->animations) ? &ent.animator : nullptr;
		DrawModel(ent.model, model, a);
	}

}

void Renderer::DrawLevel()
{
	static_wrld.use();
	SetStdShaderConstants(&static_wrld);

	const Level* level = cgame->level;
	for (int m = 0; m < level->render_data.instances.size(); m++) {
		const Level::StaticInstance& sm = level->render_data.instances[m];
		ASSERT(level->render_data.embedded_meshes[sm.model_index]);
		const Model& model = *level->render_data.embedded_meshes[sm.model_index];

		static_wrld.set_mat4("Model", sm.transform);
		static_wrld.set_mat4("InverseModel", glm::inverse(sm.transform));


		for (int p = 0; p < model.parts.size(); p++) {
			const MeshPart& mp = model.parts[p];

			if (mp.material_idx != -1) {
				const auto& mm = model.materials[mp.material_idx];
				if (mm.t1)
					BindTexture(0, mm.t1->gl_id);
				else
					BindTexture(0, white_texture);
			}
			else
				BindTexture(0, white_texture);

			glBindVertexArray(mp.vao);
			glDrawElements(GL_TRIANGLES, mp.element_count, mp.element_type, (void*)mp.element_offset);
		}
	}
}


void Renderer::AddPlayerDebugCapsule(const EntityState* es, MeshBuilder* mb, Color32 color)
{
	vec3 origin = es->position;
	Capsule c;
	c.base = origin;
	c.tip = origin + vec3(0, (es->ducking) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT, 0);
	c.radius = CHAR_HITBOX_RADIUS;
	float radius = CHAR_HITBOX_RADIUS;
	vec3 a, b;
	c.GetSphereCenters(a, b);
	mb->AddSphere(a, radius, 10, 7, color);
	mb->AddSphere(b, radius, 10, 7, color);
	mb->AddSphere((a + b) / 2.f, (c.tip.y - c.base.y)/2.f, 10, 7, COLOR_RED);
}

void Renderer::DrawPlayerViewmodel()
{
	mat4 invview = glm::inverse(vs.view_mat);

	mat4 model2 = glm::translate(invview, vec3(0.18, -0.18, -0.25) + cgame->viewmodel_offsets + cgame->viewmodel_recoil_ofs);
	model2 = model2 * glm::eulerAngleY(PI + PI / 128.f);

	cur_shader = -1;
	DrawModel(media.gamemodels[Mod_GunM16], model2);
}

void LoadMediaFiles()
{
	auto& models = media.gamemodels;
	models.resize(Mod_NUMMODELS);
	
	models[Mod_PlayerCT] = FindOrLoadModel("CT.glb");
	models[Mod_GunM16] = FindOrLoadModel("m16.glb");
	models[Mod_Grenade_HE] = FindOrLoadModel("grenade_he.glb");

	media.blobshadow = FindOrLoadTexture("blob_shadow_temp.png");
}

void DrawScreen(double dt)
{
	glCheckError();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (client.IsInGame())
		rndr.FrameDraw();
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
			if (scancode == SDL_SCANCODE_G) {
				cfg.LoadFromDisk("config.ini");
			}
			if (scancode == SDL_SCANCODE_V) {
				ReloadModel(media.gamemodels[Mod_PlayerCT]);
			}
			if (scancode == SDL_SCANCODE_APOSTROPHE) {
				Game* g = &game;
				g->KillEnt(g->ents.data() + 0);


			}
			if (scancode == SDL_SCANCODE_T)
				client.cl_game.third_person = !client.cl_game.third_person;
			if (scancode == SDL_SCANCODE_Q)
				game.paused = !game.paused;
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
				ip.port = cfg.GetI("host_port");
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

	int host_port = DEFAULT_SERVER_PORT;

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
	core.tick_interval = 1.0 / DEFAULT_UPDATE_RATE;

	CreateWindow(startx,starty);

	cfg.LoadFromDisk("config.ini");
	cfg.MakeI("host_port", DEFAULT_SERVER_PORT);
	cfg.MakeF("max_ground_speed", 10.f);

	NetworkInit();
	rndr.Init();
	LoadMediaFiles();

	if (!run_client_only) {
		server.Init();
		server.Spawn(map_file);
	}

	client.Init();

	if (run_client_only) {
		printf("Running client only\n");
		IPAndPort ip;

		ip.SetIp(127, 0, 0, 1);
		ip.port = cfg.GetI("host_port");

		client.Connect(ip);
	}


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
	}
	
	return 0;
}