#pragma once
#include "Config.h"
#include "glm/glm.hpp"
#include "Util.h"
#include "Types.h"
class Model;
class Animator;
class Texture;
class Entity;

const int BLOOM_MIPS = 8;

class Renderer
{
public:
	void Init();
	void FrameDraw();

	void render_level_to_target(View_Setup setup, uint32_t output_framebuffer, bool clear, bool draw_ents);

	void draw_text();
	void draw_rect(int x, int y, int width, int height, Color32 color, Texture* texture=nullptr, 
		float srcw=0, float srch=0, float srcx=0, float srcy=0);	// src* are in pixel coords

	void reload_shaders();
	void ui_render();

	void on_level_start();

	void DrawModel(const Model* m, glm::mat4 transform, const Animator* a = nullptr, float rough=1.0, float metal=0.0);
	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

	uint32_t white_texture;
	uint32_t black_texture;
	
	enum {
		BASE0_SAMPLER, AUX0_SAMPLER, BASE1_SAMPLER,
		AUX1_SAMPLER, SPECIAL_SAMPLER, LIGHTMAP_SAMPLER, NUM_SAMPLERS
	};

	// shaders
	enum { 
		// meshbuilder's
		S_SIMPLE, S_TEXTURED, 
		
		// model's
		S_ANIMATED, 
		S_STATIC, S_STATIC_AT, S_WIND, S_WIND_AT,
		S_LIGHTMAPPED, S_LIGHTMAPPED_AT, S_LIGHTMAPPED_BLEND2,

		// z-prepass'
		S_PREPASS, S_AT_PREPASS, S_ANIMATED_PREPASS, S_WIND_PREPASS, S_WIND_AT_PREPASS,
		
		S_PARTICLE_BASIC,

		// other
		S_BLOOM_DOWNSAMPLE, S_BLOOM_UPSAMPLE, S_COMBINE,

		S_NUM
	};

	Shader shade[S_NUM];

	struct {
		uint32_t scene;

		uint32_t bloom;
	}fbo;
	struct {
		uint32_t scene_color;
		uint32_t scene_depthstencil;

		uint32_t bloom_depth;
		uint32_t bloom_chain[BLOOM_MIPS];
		glm::ivec2 bloom_chain_isize[BLOOM_MIPS];
		glm::vec2 bloom_chain_size[BLOOM_MIPS];

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

	void draw_sprite(glm::vec3 pos, Color32 color, glm::vec2 size, Texture* mat, 
		bool billboard, bool in_world_space, bool additive, glm::vec3 orient_face);

	Shader& shader();
	void set_shader_constants();

	// >>> PBR BRANCH
	EnvCubemap cubemap;
	float rough = 1.f;
	float metal = 0.f;

private:
	struct Sprite_Drawing_State {
		bool force_set = true;
		bool in_world_space = false;
		bool additive = false;
		uint32_t current_t = 0;
	}sprite_state;
	void draw_sprite_buffer();

	struct Model_Drawing_State {
		bool is_transparent_pass = false;
		bool initial_set = false;
		bool is_lightmapped = false;
		bool is_animated = false;
		bool is_alpha_tested = false;
		bool backfaces = false;
		int alpha_type = -1;
		int shader_type = -1;

		bool set_model_params = false;
		// light, cubemap, decal list
	};
	void draw_model_real(Model_Drawing_State* state, const Model* m, int part, glm::mat4 transform, 
		const Entity* e = nullptr, const Animator* a = nullptr, Game_Shader* override_mat = nullptr);

	void init_bloom_buffers();
	void render_bloom_chain();

	void InitGlState();
	void InitFramebuffers();

	void DrawEnts();
	void DrawLevel();
	void DrawPlayerViewmodel();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void set_shader_sampler_locations();
	void set_wind_constants();

	int cur_w = 0;
	int cur_h = 0;
	uint32_t cur_shader = 0;
	uint32_t cur_tex[NUM_SAMPLERS];

	MeshBuilder ui_builder;
	uint32_t building_ui_texture;

	MeshBuilder shadowverts;

};

extern Renderer draw;