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
#include "Game_Engine.h"
#include "Types.h"
#include "Client.h"
#include "Server.h"
#include "Movement.h"
#include "Config.h"
#include "Media.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

ImGuiContext* imgui_ctx;
MeshBuilder phys_debug;
Engine_Config cfg;
Media media;
Game_Engine engine;

static double program_time_start;

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
	return true;
}

void Quit()
{
	printf("Quiting...\n");
	engine.cleanup();
	exit(0);
}
void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	fflush(stdout);
	engine.cleanup();
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


void Game_Local::update_view()
{
	if (engine.engine_state != SPAWNED)
		return;

	// decay view_recoil
	view_recoil.x = view_recoil.x * 0.9f;

	vec3 true_viewangles = engine.local.view_angles + view_recoil;

	if (view_recoil.y != 0)
		printf("view_recoil: %f\n", view_recoil.y);

	vec3 true_front = AnglesToVector(true_viewangles.x, true_viewangles.y);

	View_Setup setup;
	setup.height = engine.window_h->integer;
	setup.width = engine.window_w->integer;
	setup.fov = glm::radians(fov->real);
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, 0.01f, 100.0f);

	//if (update_camera) {
	//	fly_cam.UpdateFromInput(engine.keys, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	//}

	if (thirdperson_camera->integer) {
		//ClientEntity* player = client.GetLocalPlayer();
		Entity& playerreal = engine.local_player();

		vec3 view_angles = engine.local.view_angles;

		vec3 front = AnglesToVector(view_angles.x, view_angles.y);
		//fly_cam.position = GetLocalPlayerInterpedOrigin()+vec3(0,0.5,0) - front * 3.f;
		fly_cam.position = playerreal.position + vec3(0, STANDING_EYE_OFFSET, 0) - front * 4.5f;
		setup.view = glm::lookAt(fly_cam.position, fly_cam.position + front, fly_cam.up);
		setup.front = front;
		setup.origin = fly_cam.position;
	}
	else
	{
		Entity& player = engine.local_player();
		float view_height = (player.ducking) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
		vec3 cam_position = player.position + vec3(0, view_height, 0);
		setup.view = glm::lookAt(cam_position, cam_position + true_front, vec3(0, 1, 0));
		setup.origin = cam_position;
		setup.front = true_front;
	}

	setup.viewproj = setup.proj * setup.view;
	last_view = setup;

}

static void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}


void Game_Engine::view_angle_update()
{
	int x, y;
	SDL_GetRelativeMouseState(&x, &y);
	float x_off = engine.mouse_sensitivity->real * x;
	float y_off = engine.mouse_sensitivity->real * y;

	vec3 view_angles = local.view_angles;
	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);
	local.view_angles = view_angles;
}

void Game_Engine::make_move()
{
	MoveCommand command;
	command.view_angles = local.view_angles;
	command.tick = tick;

	if (!game_focused) {
		local.last_command = command;
		if(cl) cl->get_command(cl->GetCurrentSequence()) = command;
		return;
	}

	view_angle_update();
	if (keys[SDL_SCANCODE_W])
		command.forward_move += 1.f;
	if (keys[SDL_SCANCODE_S])
		command.forward_move -= 1.f;
	if (keys[SDL_SCANCODE_A])
		command.lateral_move += 1.f;
	if (keys[SDL_SCANCODE_D])
		command.lateral_move -= 1.f;
	if (keys[SDL_SCANCODE_SPACE])
		command.button_mask |= CmdBtn_Jump;
	if (keys[SDL_SCANCODE_LSHIFT])
		command.button_mask |= CmdBtn_Duck;
	if (keys[SDL_SCANCODE_Q])
		command.button_mask |= CmdBtn_PFire;
	if (keys[SDL_SCANCODE_E])
		command.button_mask |= CmdBtn_Reload;
	if (keys[SDL_SCANCODE_Z])
		command.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		command.up_move -= 1.f;

	// quantize and unquantize for local prediction
	command.forward_move	= MoveCommand::unquantize(MoveCommand::quantize(command.forward_move));
	command.lateral_move	= MoveCommand::unquantize(MoveCommand::quantize(command.lateral_move));
	command.up_move			= MoveCommand::unquantize(MoveCommand::quantize(command.up_move));
	
	// FIXME:
	local.last_command = command;
	if(cl) cl->get_command(cl->GetCurrentSequence()) = command;
}

void Game_Engine::init_sdl_window()
{
	ASSERT(!window);

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	program_time_start = GetTime();

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window_w->integer, window_h->integer, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	gl_context = SDL_GL_CreateContext(window);
	printf(__FUNCTION__": OpenGL loaded\n");
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
	void Init();
	void FrameDraw();

	void draw_text();
	void draw_rect();

	void DrawModel(const Model* m, mat4 transform, const Animator* a = nullptr);
	void AddPlayerDebugCapsule(const EntityState* es, MeshBuilder* mb, Color32 color);

	// shaders
	Shader simple;
	Shader textured;
	Shader animated;
	Shader static_wrld;
	Shader basic_mod;
	Shader particle_basic;

	uint32_t white_texture;
	uint32_t black_texture;

	RenderFBOs fbo;
	RenderAttachments textures;

	// config vars
	Config_Var* cfg_draw_collision_tris;
	Config_Var* cfg_draw_sv_colliders;
	Config_Var* cfg_draw_viewmodel;
	Config_Var* cfg_vsync;
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
	View_Setup vs;

	int cur_img_1 = -1;

	MeshBuilder shadowverts;

	Game_Local* gamel;
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

Renderer draw;

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

	s->set_vec3("view_front", vs.front);
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

	cfg_draw_collision_tris = cfg.get_var("draw_collision_tris", "0");
	cfg_draw_sv_colliders	= cfg.get_var("draw_sv_colliders", "0");
	cfg_draw_viewmodel		= cfg.get_var("draw_viewmodel", "1");
	cfg_vsync				= cfg.get_var("vsync", "0");

	fbo.scene = fbo.ssao = 0;
	textures.scene_color = textures.scene_depthstencil = textures.ssao_color = 0;
	InitFramebuffers();

	gamel = &engine.local;
}

void Renderer::InitFramebuffers()
{
	const int s_w = engine.window_w->integer;
	const int s_h = engine.window_h->integer;

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
	if (cur_w != engine.window_w->integer || cur_h != engine.window_h->integer)
		InitFramebuffers();

	vs = gamel->last_view;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	glViewport(0, 0, vs.width, vs.height);
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
	if (IsServerActive() && cfg_draw_sv_colliders->integer == 1) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			// FIXME: leaking server code into client code
			if (engine.ents[i].type == Ent_Player) {
				EntityState es = engine.ents[i].ToEntState();
				AddPlayerDebugCapsule(&es, &mb, COLOR_CYAN);
			}
		}
	}

	mb.End();
	simple.use();
	simple.set_mat4("ViewProj", vs.viewproj);
	simple.set_mat4("Model", mat4(1.f));

	if(cfg_draw_collision_tris->integer)
		DrawCollisionWorld(engine.level);

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


	if(!gamel->thirdperson_camera->integer && cfg_draw_viewmodel->integer)
		DrawPlayerViewmodel();
}

void Renderer::DrawEntBlobShadows()
{
	shadowverts.Begin();

	for (int i = 0; i <MAX_GAME_ENTS; i++)
	{
		Entity* e = &engine.ents[i];
		if (!e->active()) continue;
		
		RayHit rh;
		Ray r;
		r.pos = e->position + glm::vec3(0,0.1f,0);
		r.dir = glm::vec3(0, -1, 0);
		engine.phys.TraceRay(r, &rh, i, Pf_World);

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
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		auto& ent = engine.ents[i];
		//auto& ent = cgame->entities[i];
		if (!ent.active())
			continue;
		if (!ent.model)
			continue;

		if (i == engine.player_num() && !gamel->thirdperson_camera->integer)
			continue;

		//EntityState* cur = &ent.interpstate;

		mat4 model = glm::translate(mat4(1), ent.position);
		model = model * glm::eulerAngleXYZ(ent.rotation.x, ent.rotation.y, ent.rotation.z);
		model = glm::scale(model, vec3(1.f));

		const Animator* a = (ent.model->animations) ? &ent.anim : nullptr;
		DrawModel(ent.model, model, a);
	}

}

void Renderer::DrawLevel()
{
	static_wrld.use();
	SetStdShaderConstants(&static_wrld);

	const Level* level = engine.level;
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
	mat4 invview = glm::inverse(vs.view);

	mat4 model2 = glm::translate(invview, vec3(0.18, -0.18, -0.25) + gamel->viewmodel_offsets + gamel->viewmodel_recoil_ofs);
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

int Game_Engine::player_num()
{
	if (is_host)
		return 0;
	if (cl)
		return cl->GetPlayerNum();
	ASSERT(0);
	return 0;
}
Entity& Game_Engine::local_player()
{
	ASSERT(engine_state >= LOADING);
	return ents[player_num()];
}

void Game_Engine::connect_to(string address)
{
	if (is_host)
		exit_map();
	if (cl)
		end_client();

	startup_client();	// (cl = new Client)
	cl->connect(address);
}

void cmd_quit()
{
	Quit();
}
void cmd_bind()
{
	auto& args = cfg.get_arg_list();
	if (args.size() < 2) return;
	int scancode = SDL_GetScancodeFromName(args[1].c_str());
	if (scancode == SDL_SCANCODE_UNKNOWN) return;
	if (args.size() < 3)
		engine.bind_key(scancode, "");
	else
		engine.bind_key(scancode, args[2].c_str());
}

void cmd_server_end()
{
	engine.exit_map();
}
void cmd_client_connect()
{
	auto& args = cfg.get_arg_list();
	if (args.size() < 2) return;

	engine.connect_to(args.at(1));
}
void cmd_client_disconnect()
{
	if (engine.cl) {
		engine.cl->Disconnect();
	}

	//client.Disconnect();
}
void cmd_client_reconnect()
{
	if(engine.cl)
		engine.cl->Reconnect();
}
void cmd_load_map()
{
	if (cfg.get_arg_list().size() < 2) {
		printf(__FUNCTION__": needs map\n");
	}

	engine.start_map(cfg.get_arg_list().at(1), false);
}
void cmd_game_input_callback()
{

}

int main(int argc, char** argv)
{
	printf("Starting engine...\n");

	engine.argc = argc;
	engine.argv = argv;
	engine.init();

	engine.loop();

	engine.cleanup();
	
	return 0;
}

void Game_Local::init()
{
	view_angles = glm::vec3(0.f);
	thirdperson_camera = cfg.get_var("thirdperson_camera", "1");
	fov = cfg.get_var("fov", "70.0");
}

void Game_Engine::start_map(string map, bool is_client)
{
	// starting new map
	mapname = map;
	FreeLevel(level);
	phys.ClearObjs();
	for (int i = 0; i < MAX_GAME_ENTS; i++)
		ents[i] = Entity();
	num_entities = 0;
	level = LoadLevelFile(mapname.c_str());
	if (!level) 
		return;

	tick = 0;
	time = 0;
	
	if (!is_client) {
		server.start();
		engine.is_host = true;
		engine.engine_state = SPAWNED;
		server.connect_local_client();
	}
	//client.server_mgr.force_into_local_game();
}

void Game_Engine::set_tick_rate(float tick_rate)
{
	if (is_host) {
		printf("can't change tick rate while server is running\n");
		return;
	}
	tick_interval = 1.0 / tick_rate;
}

void Game_Engine::exit_map()
{
	FreeLevel(level);
	level = nullptr;

	server.end();

	engine_state = MAINMENU;
	is_host = false;
	//client.Disconnect();
}


void Game_Engine::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN) {
		int scancode = event.key.keysym.scancode;
		keys[scancode] = true;

		if (binds[scancode]) {
			cfg.execute(*binds[scancode]);
		}
	}
	else if (event.type == SDL_KEYUP) {
		keys[event.key.keysym.scancode] = false;
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		int x, y;
		SDL_GetRelativeMouseState(&x, &y);
		engine.game_focused = true;
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		engine.game_focused = false;
	}
}

void Game_Engine::bind_key(int key, string command)
{
	ASSERT(key >= 0 && key < SDL_NUM_SCANCODES);
	if (!binds[key])
		binds[key] = new string;
	*binds[key] = std::move(command);
}

void Game_Engine::cleanup()
{
	FreeLoadedModels();
	FreeLoadedTextures();
	NetworkQuit();
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);

	for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
		delete binds[i];
	}
}

void Game_Engine::draw_screen()
{
	glCheckError();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT);
	if (engine_state == SPAWNED)
		draw.FrameDraw();
	glCheckError();
	SDL_GL_SwapWindow(window);
	glCheckError();
}

void Game_Engine::build_physics_world(float time)
{
	phys.ClearObjs();
	phys.AddLevel(level);

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& ce = ents[i];
		if (ce.type != Ent_Player) continue;

		CharacterShape cs;
		cs.a = &ce.anim;
		cs.m = ce.model;
		cs.org = ce.position;
		cs.radius = CHAR_HITBOX_RADIUS;
		cs.height = (!ce.ducking) ? CHAR_STANDING_HB_HEIGHT : CHAR_CROUCING_HB_HEIGHT;
		PhysicsObject po;
		po.shape = PhysicsObject::Character;
		po.character = cs;
		po.userindex = i;
		po.player = true;

		phys.AddObj(po);
	}
}

void PlayerUpdate(Entity* e);
void DummyUpdate(Entity* e);
void GrenadeUpdate(Entity* e);


void Game_Engine::update_game_tick()
{
	// update local player
	game.ExecutePlayerMove(&ents[0], local.last_command);
	game.BuildPhysicsWorld(0.f);

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity* e = &ents[i];
		if (e->type == Ent_Free)
			continue;

		switch (e->type) {
		case Ent_Player:
			PlayerUpdate(e);
			break;
		case Ent_Dummy:
			DummyUpdate(e);
			break;
		case Ent_Grenade:
			GrenadeUpdate(e);
			break;
		}
		e->anim.AdvanceFrame(engine.tick_interval);

	}
}


void Game_Engine::init()
{
	memset(keys, 0, sizeof(keys));
	memset(binds, 0, sizeof(binds));
	num_entities = 0;
	level = nullptr;
	cl = nullptr;
	tick_interval = 1.0 / DEFAULT_UPDATE_RATE;
	engine_state = MAINMENU;
	is_host = false;

	// config vars
	window_w			= cfg.get_var("window_w", "1200", true);
	window_h			= cfg.get_var("window_h", "800", true);
	window_fullscreen	= cfg.get_var("window_fullscreen", "0", true);
	host_port			= cfg.get_var("host_port", std::to_string(DEFAULT_SERVER_PORT).c_str());
	mouse_sensitivity	= cfg.get_var("mouse_sensitivity", "0.01");
	cfg.set_var("max_ground_speed", "10.0");	// ???

	
	// engine commands
	cfg.set_command("connect", cmd_client_connect);
	cfg.set_command("reconnect", cmd_client_reconnect);
	cfg.set_command("disconnect", cmd_client_disconnect);
	cfg.set_command("map", cmd_load_map);
	cfg.set_command("sv_end", cmd_server_end);
	cfg.set_command("bind", cmd_bind);
	cfg.set_command("quit", cmd_quit);

	
	// engine initilization
	init_sdl_window();
	network_init();
	draw.Init();
	LoadMediaFiles();

	//client.Init();
	server.Init();

	local.init();

	// debug interface
	imgui_ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_ctx);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();

	// key binds
	bind_key(SDL_SCANCODE_Y, "thirdperson_camera 0");
	bind_key(SDL_SCANCODE_2, "reconnect");
	bind_key(SDL_SCANCODE_1, "disconnect");
	bind_key(SDL_SCANCODE_3, "sv_end");


	cfg.execute_file("vars.txt");	// load config vars
	cfg.execute_file("init.txt");	// load startup script

	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;
	std::vector<string> buffered_commands;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-x") == 0) {
			startx = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-y") == 0) {
			starty = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			cfg.set_var("window_w", argv[++i]);
		}
		else if (strcmp(argv[i], "-h") == 0) {
			cfg.set_var("window_h", argv[++i]);
		}
		else if (argv[i][0] == '-') {
			string cmd;
			cmd += &argv[i++][1];
			while (i < argc && argv[i][0] != '-') {
				cmd += ' ';
				cmd += argv[i++];
			}
			buffered_commands.push_back(cmd);
		}
	}

	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, window_w->integer, window_h->integer);
	for (const auto& cmd : buffered_commands)
		cfg.execute(cmd);

	cfg.print_vars();
}

void Game_Engine::startup_client()
{
	cl = new Client;
	cl->Init();
}

void Game_Engine::end_client()
{
	cl->Disconnect();
	delete cl;
	cl = nullptr;
}


void Game_Engine::loop()
{
	double last = GetTime() - 0.1;
	for (;;)
	{
		double now = GetTime();
		double dt = now - last;
		last = now;
		if (dt > 0.1)
			dt = 0.1;
		frame_time = dt;

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type) {
			case SDL_QUIT:
				::Quit();
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
				key_event(event);
				break;
			}
		}

		double secs_per_tick = tick_interval;
		frame_remainder += dt;
		int num_ticks = (int)floor(frame_remainder / secs_per_tick);
		frame_remainder -= num_ticks * secs_per_tick;

		/*
		make input (both listen and clients)
		client send input to server (listens do not)

		listens read packets from clients
		listens update game (sim local character with frame's input)
		listen build a snapshot frame for sending to clients

		listens dont have packets to "read" from server, stuff like sounds/particles are just branched when they are created on sim frames
		listens dont run prediction for local player
		*/

		for (int i = 0; i < num_ticks; i++)
		{
			if (engine_state >= LOADING)
				make_move();

			if (!is_host && cl) {
				cl->server_mgr.SendMovesAndMessages();
				cl->server_mgr.TrySendingConnect();
			}

			time = tick * tick_interval;
			if (is_host) {
				//server.simtime = tick * engine.tick_interval;
				server.ReadPackets();

				update_game_tick();

				server.BuildSnapshotFrame();
				for (int i = 0; i < server.clients.size(); i++)
					server.clients[i].Update();
				tick += 1;
				//server.tick += 1;
			}


			if (!is_host && cl) {
				if (cl->get_state() == CS_SPAWNED) {
					tick += 1;
					time = tick * tick_interval;
					//cl->tick += 1;
					//cl->time = cl->tick * engine.tick_interval;
				}
				cl->server_mgr.ReadPackets();
				cl->run_prediction();

				switch (cl->get_state()) {
				case CS_DISCONNECTED:
				case CS_TRYINGCONNECT:engine_state = MAINMENU; break;
				case CS_CONNECTED: engine_state = LOADING; break;
				case CS_SPAWNED: engine_state = SPAWNED; break;
				}
			}
		}
		pre_render_update();
		draw_screen();
	}
}

void Game_Engine::pre_render_update()
{
	if (engine_state == SPAWNED) {
		// interpolate entities for rendering
		if (!is_host)
			cl->interpolate_states();

		//cl_game.ComputeAnimationMatricies();
		for (int i = 0; i < MAX_GAME_ENTS; i++) {
			Entity& ent = ents[i];
			if (!ent.active())
				continue;
			if (!ent.model || !ent.model->animations)
				continue;

			ent.anim.mainanim = ent.anim.leganim;
			ent.anim.mainanim_frame = ent.anim.leganim_frame;

			ent.anim.SetupBones();
			ent.anim.ConcatWithInvPose();
		}

		local.update_viewmodel();
	}
	local.update_view();
}