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
#include "Player.h"
#include "Config.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"

MeshBuilder phys_debug;
Engine_Config cfg;
Game_Engine engine;
Game_Media media;

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
		sys_print("%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
}

void Quit()
{
	sys_print("Quiting...\n");
	engine.cleanup();
	exit(0);
}
void sys_print(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	engine.console.print_args(fmt, args);
	va_end(args);
}

void sys_vprint(const char* fmt, va_list args)
{
	vprintf(fmt, args);
	engine.console.print_args(fmt, args);
}

void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	sys_vprint(format, list);
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

void Game_Local::update_view()
{

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
		float view_height = (player.state & PMS_CROUCHING) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
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
	float x_off = engine.local.mouse_sensitivity->real * x;
	float y_off = engine.local.mouse_sensitivity->real * y;

	vec3 view_angles = local.view_angles;
	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);
	local.view_angles = view_angles;
}

void Game_Engine::make_move()
{
	Move_Command command;
	command.view_angles = local.view_angles;
	command.tick = tick;

	if (engine.local.fake_movement_debug->integer != 0)
		command.lateral_move = std::fmod(GetTime(), 2.f) > 1.f ? -1.f : 1.f;
	if (engine.local.fake_movement_debug->integer == 2)
		command.button_mask |= BUTTON_JUMP;

	if (!game_focused) {
		local.last_command = command;
		if(cl->get_state()>=CS_CONNECTED) cl->get_command(cl->OutSequence()) = command;
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
	if (keys[SDL_SCANCODE_Z])
		command.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		command.up_move -= 1.f;
	if (keys[SDL_SCANCODE_SPACE])
		command.button_mask |= BUTTON_JUMP;
	if (keys[SDL_SCANCODE_LSHIFT])
		command.button_mask |= BUTTON_DUCK;
	if (keys[SDL_SCANCODE_Q])
		command.button_mask |= BUTTON_FIRE1;
	if (keys[SDL_SCANCODE_E])
		command.button_mask |= BUTTON_RELOAD;

	// quantize and unquantize for local prediction
	command.forward_move	= Move_Command::unquantize(Move_Command::quantize(command.forward_move));
	command.lateral_move	= Move_Command::unquantize(Move_Command::quantize(command.lateral_move));
	command.up_move			= Move_Command::unquantize(Move_Command::quantize(command.up_move));
	
	// FIXME:
	local.last_command = command;
	if(cl->get_state()>=CS_CONNECTED) cl->get_command(cl->OutSequence()) = command;
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
	sys_print("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	sys_print("Vendor: %s\n", glGetString(GL_VENDOR));
	sys_print("Renderer: %s\n", glGetString(GL_RENDERER));
	sys_print("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);
}

glm::mat4 Fly_Camera::get_view_matrix() const {
	return glm::lookAt(position, position + front, up);
}

void Fly_Camera::scroll_speed(int amt)
{
	move_speed += (move_speed * 0.5) * amt;
	if (abs(move_speed) < 0.000001)
		move_speed = 0.0001;
}
void Fly_Camera::update_from_input(const bool keys[], int mouse_dx, int mouse_dy)
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
	front = AnglesToVector(pitch, yaw);

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
	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

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
	Config_Var* r_draw_collision_tris;
	Config_Var* r_draw_sv_colliders;
	Config_Var* r_draw_viewmodel;
	Config_Var* vsync;
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

	r_draw_collision_tris	= cfg.get_var("draw_collision_tris", "0");
	r_draw_sv_colliders		= cfg.get_var("draw_sv_colliders", "0");
	r_draw_viewmodel		= cfg.get_var("draw_viewmodel", "1");
	vsync					= cfg.get_var("vsync", "1");

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
	if (r_draw_sv_colliders->integer == 1) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (engine.ents[i].type == ET_PLAYER) {
				AddPlayerDebugCapsule(engine.ents[i], &mb, COLOR_CYAN);
			}
		}
	}

	mb.End();
	simple.use();
	simple.set_mat4("ViewProj", vs.viewproj);
	simple.set_mat4("Model", mat4(1.f));

	if(r_draw_collision_tris->integer)
		DrawCollisionWorld(engine.level);

	mb.Draw(GL_LINES);
	//game.rays.End();
	//game.rays.Draw(GL_LINES);
	if (engine.is_host) {
		phys_debug.End();
		phys_debug.Draw(GL_LINES);
	}
	mb.Free();

	glCheckError();
	glClear(GL_DEPTH_BUFFER_BIT);


	if(!gamel->thirdperson_camera->integer && r_draw_viewmodel->integer)
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
		engine.phys.TraceRay(r, &rh, i, PF_WORLD);

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

	BindTexture(0, media.blob_shadow->gl_id);
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


void Renderer::AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color)
{
	vec3 origin = e.position;
	Capsule c;
	c.base = origin;
	c.tip = origin + vec3(0, (e.solid) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT, 0);
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

	const Model* viewmodel = media.get_game_model("m16.glb");

	DrawModel(viewmodel, model2);
}

const char* game_sound_names[] =
{
	"gunshot1.wav",
};

const char* game_model_names[] =
{
	"player_character.glb",
	"m16.glb",
	"grenade_he.glb"
};
const int NUM_GAME_MODELS = sizeof(game_model_names) / sizeof(char*);


void Game_Media::load()
{
	game_models.resize(NUM_GAME_MODELS);
	for (int i = 0; i < NUM_GAME_MODELS; i++)
		game_models[i] = FindOrLoadModel(game_model_names[i]);

	blob_shadow = FindOrLoadTexture("blob_shadow_temp.png");
}

const Model* Game_Media::get_game_model(const char* model, int* out_index)
{
	int i = 0;
	for (; i < NUM_GAME_MODELS; i++) {
		if (strcmp(game_model_names[i], model) == 0)
			break;
	}

	if (i == NUM_GAME_MODELS) {
		if (out_index) *out_index = -1;
		return nullptr;
	}
	else {
		if (out_index) *out_index = i;
		return game_models[i];
	}
}
const Model* Game_Media::get_game_model_from_index(int index)
{
	if (index >= 0 && index < game_models.size())
		return game_models[index];
	return nullptr;
}

void Entity::set_model(const char* model_name)
{
	model = media.get_game_model(model_name, &model_index);
	if (model && model->bones.size() > 0)
		anim.set_model(model);
}


struct Sound
{
	char* buffer;
	int length;
};

int Game_Engine::player_num()
{
	if (is_host)
		return 0;
	if (cl && cl->client_num != -1)
		return cl->client_num;
	ASSERT(0 && "player num called without game running");
	return 0;
}
Entity& Game_Engine::local_player()
{
	//ASSERT(engine_state >= LOADING);
	return ents[player_num()];
}

void Game_Engine::connect_to(string address)
{
	if (is_host)
		exit_map();
	else if (cl->get_state() != CS_DISCONNECTED)
		cl->Disconnect("connecting to another server");
	sys_print("Connecting to server %s\n", address.c_str());
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
void cmd_client_force_update()
{
	engine.cl->ForceFullUpdate();
}
void cmd_client_connect()
{
	auto& args = cfg.get_arg_list();
	if (args.size() < 2) {
		sys_print("usage connect <address>");
		return;
	}

	engine.connect_to(args.at(1));
}
void cmd_client_disconnect()
{
	if (engine.cl->get_state()!=CS_DISCONNECTED)
		engine.cl->Disconnect("requested");
}
void cmd_client_reconnect()
{
	if(engine.cl->get_state() != CS_DISCONNECTED)
		engine.cl->Reconnect();
}
void cmd_load_map()
{
	if (cfg.get_arg_list().size() < 2) {
		sys_print("usage map <map name>");
		return;
	}

	engine.start_map(cfg.get_arg_list().at(1), false);
}
void cmd_exec_file()
{
	if (cfg.get_arg_list().size() < 2) {
		sys_print("usage map <map name>");
		return;
	}
	cfg.execute_file(cfg.get_arg_list().at(1).c_str());
}

void cmd_print_client_net_stats()
{
	float mintime = INFINITY;
	float maxtime = -INFINITY;
	int maxbytes = -5000;
	int totalbytes = 0;
	for (int i = 0; i < 64; i++) {
		auto& entry = engine.cl->server.incoming[i];
		maxbytes = glm::max(maxbytes, entry.bytes);
		totalbytes += entry.bytes;
		mintime = glm::min(mintime, entry.time);
		maxtime = glm::max(maxtime, entry.time);
	}

	sys_print("Client Network Stats:\n");
	sys_print("%--15s %f\n", "Rtt", engine.cl->server.rtt);
	sys_print("%--15s %f\n", "Interval", maxtime - mintime);
	sys_print("%--15s %d\n", "Biggest packet", maxbytes);
	sys_print("%--15s %f\n", "Kbits/s", 8.f*(totalbytes / (maxtime-mintime))/1000.f);
	sys_print("%--15s %f\n", "Bytes/Packet", totalbytes / 64.0);
}

void cmd_print_entities()
{
	sys_print("%--15s %--15s %--15s %--15s\n", "index", "class", "posx", "posz", "has_model");
	for (int i = 0; i < NUM_GAME_ENTS; i++) {
		auto& e = engine.ents[i];
		if (!e.active()) continue;
		sys_print("%-15d %-15d %-15f %-15f %-15d\n", i, e.type, e.position.x, e.position.z, (int)e.model);
	}
}

void cmd_print_vars()
{
	cfg.print_vars();
}

int main(int argc, char** argv)
{
	new_entity_fields_test();

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
	thirdperson_camera	= cfg.get_var("thirdperson_camera", "1");
	fov					= cfg.get_var("fov", "70.0");
	mouse_sensitivity	= cfg.get_var("mouse_sensitivity", "0.01");
	fake_movement_debug = cfg.get_var("fake_movement_debug", "0");
	// 1 = left to right
	// 2 = forwards and back
	// 3 = 1 with jumping
	// 4 = 2 with jumping
}

bool Game_Engine::start_map(string map, bool is_client)
{
	sys_print("Starting map %s\n", map.c_str());
	mapname = map;
	FreeLevel(level);
	phys.ClearObjs();
	for (int i = 0; i < MAX_GAME_ENTS; i++)
		ents[i] = Entity();
	num_entities = 0;
	level = LoadLevelFile(mapname.c_str());
	if (!level) {
		return false;
	}
	tick = 0;
	time = 0;
	
	if (!is_client) {
		if (cl->get_state() != CS_DISCONNECTED)
			cl->Disconnect("starting a local server");

		sv->start();
		engine.is_host = true;

		engine.set_state(ENGINE_GAME);

		sv->connect_local_client();
	}

	return true;
}

void Game_Engine::set_tick_rate(float tick_rate)
{
	if (is_host) {
		sys_print("Can't change tick rate while server is running\n");
		return;
	}
	tick_interval = 1.0 / tick_rate;
}

void Game_Engine::exit_map()
{
	FreeLevel(level);
	level = nullptr;
	sv->end("exiting to menu");
	is_host = false;
}


void Game_Engine::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;
		console.set_keyboard_focus = show_console;
	}

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse)
		return;
	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

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

void Game_Engine::draw_debug_interface()
{
	if(show_console)
		console.draw();
	move_variables_menu();


	if (state == ENGINE_GAME) {

		// player menu
		if (ImGui::Begin("player")) {
			Entity& p = engine.local_player();
			ImGui::DragFloat3("pos", &p.position.x);
			ImGui::DragFloat3("vel", &p.velocity.x);
			ImGui::LabelText("jump", "%d", bool(p.state & PMS_JUMPING));
		}
		ImGui::End();
		if (is_host) {
			if (ImGui::Begin("client")) {
				ImGui::Text("delta %f", cl->time_delta);
				ImGui::Text("tick %f", engine.tick);
				ImGui::Text("frr %f", engine.frame_remainder);
			}
			ImGui::End();
		}
	}
}

void Game_Engine::draw_screen()
{
	glCheckError();
	int x, y;
	SDL_GetWindowSize(window, &x, &y);
	cfg.set_var("window_w", std::to_string(x).c_str());
	cfg.set_var("window_h", std::to_string(y).c_str());
	if (draw.vsync->integer != 0 && draw.vsync->integer != 1)
		cfg.set_var("vsync", "0");
	SDL_GL_SetSwapInterval(draw.vsync->integer);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT);
	if (state == ENGINE_GAME)
		draw.FrameDraw();

	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	draw_debug_interface();
	//ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glCheckError();
	SDL_GL_SwapWindow(window);
	glCheckError();
}

void Game_Engine::set_state(Engine_State state)
{
	const char* engine_strs[] = { "menu", "loading", "game" };

	if (this->state != state)
		sys_print("Engine going to %s state\n", engine_strs[(int)state]);
	this->state = state;
}

void Game_Engine::build_physics_world(float time)
{
	phys.ClearObjs();
	phys.AddLevel(level);

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& ce = ents[i];
		if (ce.type != ET_PLAYER) continue;

		CharacterShape cs;
		cs.a = &ce.anim;
		cs.m = ce.model;
		cs.org = ce.position;
		cs.radius = CHAR_HITBOX_RADIUS;
		cs.height = (!(ce.state & PMS_CROUCHING)) ? CHAR_STANDING_HB_HEIGHT : CHAR_CROUCING_HB_HEIGHT;
		PhysicsObject po;
		po.shape = PhysicsObject::Character;
		po.character = cs;
		po.userindex = i;
		po.player = true;

		phys.AddObj(po);
	}
}

void player_update(Entity* e);
void DummyUpdate(Entity* e);
void grenade_update(Entity* e);


void Game_Engine::update_game_tick()
{
	// update local player
	execute_player_move(0, local.last_command);

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& e = ents[i];
		if (!ents[i].active())
			continue;

		if (e.timer > 0.f) {
			e.timer -= engine.tick_interval;
			if (e.timer <= 0.f && e.timer_callback)
				e.timer_callback(&e);
		}

		e.physics_update();
		if (e.update)
			e.update(&e);
		if(e.model && e.model->animations)
			e.anim.AdvanceFrame(tick_interval);
	}
}

void cmd_debug_counter()
{
	if (cfg.get_arg_list().at(1) == "1")
		engine.cl->offset_debug++;
	else
		engine.cl->offset_debug--;
	printf("offset client: %d\n", engine.cl->offset_debug);
}

void Game_Engine::init()
{
	memset(keys, 0, sizeof(keys));
	memset(binds, 0, sizeof(binds));
	num_entities = 0;
	level = nullptr;
	tick_interval = 1.0 / DEFAULT_UPDATE_RATE;
	state = ENGINE_MENU;
	is_host = false;
	sv = new Server;
	cl = new Client;
	cfg.set_unknown_variables = true;

	// config vars
	window_w			= cfg.get_var("window_w", "1200", true);
	window_h			= cfg.get_var("window_h", "800", true);
	window_fullscreen	= cfg.get_var("window_fullscreen", "0", true);
	host_port			= cfg.get_var("host_port", std::to_string(DEFAULT_SERVER_PORT).c_str());

	// engine commands
	cfg.set_command("connect", cmd_client_connect);
	cfg.set_command("reconnect", cmd_client_reconnect);
	cfg.set_command("disconnect", cmd_client_disconnect);
	cfg.set_command("map", cmd_load_map);
	cfg.set_command("sv_end", cmd_server_end);
	cfg.set_command("bind", cmd_bind);
	cfg.set_command("quit", cmd_quit);
	cfg.set_command("counter", cmd_debug_counter);
	cfg.set_command("net_stat", cmd_print_client_net_stats);
	cfg.set_command("cl_full_update", cmd_client_force_update);
	cfg.set_command("print_ents", cmd_print_entities);
	cfg.set_command("print_vars", cmd_print_vars);
	cfg.set_command("exec", cmd_exec_file);

	// engine initilization
	init_sdl_window();
	network_init();
	draw.Init();
	media.load();

	cl->init();
	sv->init();
	local.init();

	// debug interface
	imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();

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
		else if (strcmp(argv[i], "-VISUALSTUDIO") == 0) {
			SDL_SetWindowTitle(window, "CsRemake - VISUAL STUDIO\n");
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

	cfg.execute_file("vars.txt");	// load config vars
	cfg.execute_file("init.txt");	// load startup script

	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, window_w->integer, window_h->integer);
	for (const auto& cmd : buffered_commands)
		cfg.execute(cmd);

	cfg.set_unknown_variables = false;
}


/*
make input (both listen and clients)
client send input to server (listens do not)

listens read packets from clients
listens update game (sim local character with frame's input)
listen build a snapshot frame for sending to clients

listens dont have packets to "read" from server, stuff like sounds/particles are just branched when they are created on sim frames
listens dont run prediction for local player
*/


void Game_Engine::game_update_tick()
{
	make_move();
	if (!is_host)
		cl->SendMovesAndMessages();

	time = tick * tick_interval;
	if (is_host) {
		// build physics world now as ReadPackets() executes player commands
		build_physics_world(0.f);
		sv->ReadPackets();
		update_game_tick();
		sv->make_snapshot();
		for (int i = 0; i < sv->clients.size(); i++)
			sv->clients[i].Update();
		tick += 1;
	}

	if (!is_host) {
		cl->ReadPackets();
		cl->run_prediction();
		tick += 1;
	}
}

void Game_Engine::loop()
{
	double last = GetTime() - 0.1;

	for (;;)
	{
		// update time
		double now = GetTime();
		double dt = now - last;
		last = now;
		if (dt > 0.1)
			dt = 0.1;
		frame_time = dt;

		// update input
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

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

		// update state
		switch (state)
		{
		case ENGINE_MENU:
			// later, will add menu controls, now all you can do is use the console to change state
			SDL_Delay(5);
			break;
		case ENGINE_LOADING:
			// here, the client tries to connect and waits for snapshot to arrive before going to game state
			cl->TrySendingConnect();
			cl->ReadPackets();
			SDL_Delay(5);
			break;
		case ENGINE_GAME: {
			ASSERT(cl->get_state() == CS_SPAWNED || is_host);

			double secs_per_tick = tick_interval;
			frame_remainder += dt;
			int num_ticks = (int)floor(frame_remainder / secs_per_tick);
			frame_remainder -= num_ticks * secs_per_tick;

			if (!is_host) {
				frame_remainder += cl->adjust_time_step(num_ticks);
			}

			for (int i = 0; i < num_ticks && state == ENGINE_GAME; i++)
				game_update_tick();

			if(state == ENGINE_GAME)
				pre_render_update();
		}break;
		}

		draw_screen();

		static float next_print = 0;
		Config_Var* print_fps = cfg.get_var("print_fps", "0");
		if (next_print <= 0 && print_fps->integer) {
			next_print += 2.0;
			sys_print("fps %f", 1.0 / engine.frame_time);
		}
		else if (print_fps->integer)
			next_print -= engine.frame_time;
	}
}

void Game_Engine::pre_render_update()
{
	ASSERT(state == ENGINE_GAME);

		// interpolate entities for rendering
	if (!is_host)
		cl->interpolate_states();

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& ent = ents[i];
		if (!ent.active())
			continue;
		if (!ent.model || !ent.model->animations)
			continue;

		ent.anim.SetupBones();
		ent.anim.ConcatWithInvPose();
	}

	local.update_viewmodel();

	local.update_view();
}


int debug_console_text_callback(ImGuiInputTextCallbackData* data)
{
	Debug_Console* console = (Debug_Console*)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		if (data->EventKey == ImGuiKey_UpArrow) {
			if (console->history_index == -1) {
				console->history_index = console->history.size() -1 ;
			}
			else {
				console->history_index--;
				if (console->history_index < 0)
					console->history_index = 0;
			}
		}
		else if (data->EventKey == ImGuiKey_DownArrow) {
			if (console->history_index != -1) {
				console->history_index++;
				if (console->history_index >= console->history.size())
					console->history_index = console->history.size() - 1;
			}
		}
		console->scroll_to_bottom = true;
		if (console->history_index != -1) {
			auto& hist = console->history[console->history_index];
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, hist.c_str());
		}
	}
	return 0;
}

void Debug_Console::draw()
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console")) {
		ImGui::End();
		return;
	}
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		for (int i = 0; i < lines.size(); i++)
		{
			ImVec4 color;
			bool has_color = false;
			if (!lines[i].empty() && lines[i][0]=='#') { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
			if (has_color)
				ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(lines[i].c_str());
			if (has_color)
				ImGui::PopStyleColor();
		}
		if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		scroll_to_bottom = false;

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::Separator();

	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
		ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory;
	if (set_keyboard_focus) {
		ImGui::SetKeyboardFocusHere();
		set_keyboard_focus = false;
	}
	if (ImGui::InputText("Input", input_buffer, IM_ARRAYSIZE(input_buffer), input_text_flags, debug_console_text_callback, this))
	{
		char* s = input_buffer;
		if (s[0]) {
			print("#%s", input_buffer);
			cfg.execute(input_buffer);
			history.push_back(input_buffer);
			scroll_to_bottom = true;

			history_index = -1;
		}
		s[0] = 0;
		reclaim_focus = true;
	}

	// Auto-focus on window apparition
	ImGui::SetItemDefaultFocus();
	if (reclaim_focus)
		ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

	ImGui::End();
}
void Debug_Console::print_args(const char* fmt, va_list args)
{
	char buf[1024];
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	lines.push_back(buf);
}

void Debug_Console::print(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	lines.push_back(buf);
}