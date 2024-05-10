#pragma once
#include <SDL2/SDL.h>
#include "Shader.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include "Framework/Vs.h"


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

class EntityState;
class Model;
class Animator;
class ClientGame;
class Renderer
{
public:
	Renderer(Shared* local) : mylocal(local) {}

	void Init();
	void FrameDraw();

	void DrawTexturedQuad();
	void DrawText();

	void DrawModel(const Model* m, glm::mat4 transform, const Animator* a = nullptr);
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
	int cur_img_1 = -1;
	ViewSetup vs;

	Shared* mylocal;

	MeshBuilder shadowverts;
};

const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;

class Shared
{
public:
	Renderer r;
	ImGuiContext* imgui_ctx;
	SDL_Window* window;
	SDL_GLContext context;
	int vid_width = DEFAULT_WIDTH;
	int vid_height = DEFAULT_HEIGHT;


	void init(int argc, char** argv);
	void main_loop(double frametime);
	void handle_input();
	void draw_screen(double frametime);
	void cleanup();
private:
	void init_debug_interface();
	void create_window(int x, int y);
};