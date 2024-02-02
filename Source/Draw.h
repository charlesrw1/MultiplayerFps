#pragma once
#include "Config.h"
#include "glm/glm.hpp"
#include "Util.h"
class Model;
class Animator;
class Entity;
class Renderer
{
public:
	void Init();
	void FrameDraw();

	void draw_text();
	void draw_rect();

	void DrawModel(const Model* m, glm::mat4 transform, const Animator* a = nullptr);
	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

	uint32_t white_texture;
	uint32_t black_texture;
	
	// shaders
	struct {
		Shader simple, textured, animated,
			static_wrld, basic_mod, particle_basic;
	}shade;

	struct {
		uint32_t scene;
	}fbo;
	struct {
		uint32_t scene_color;
		uint32_t scene_depthstencil;
	}tex;
	View_Setup vs;
	
	// config vars
	Config_Var* r_draw_collision_tris;
	Config_Var* r_draw_sv_colliders;
	Config_Var* r_draw_viewmodel;
	Config_Var* vsync;

	void bind_texture(int bind, int id);
	void set_shader(Shader& s) { 
		if (s.ID != cur_shader) {
			s.use();
			cur_shader = s.ID; 
		}
	}
	Shader& shader();
	void set_shader_constants();
private:

	void InitGlState();
	void InitFramebuffers();

	void DrawEnts();
	void DrawLevel();
	void DrawPlayerViewmodel();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);


	int cur_w = 0;
	int cur_h = 0;
	uint32_t cur_shader = 0;
	uint32_t cur_tex[4] = { 0,0,0,0 };


	MeshBuilder shadowverts;
};

extern Renderer draw;