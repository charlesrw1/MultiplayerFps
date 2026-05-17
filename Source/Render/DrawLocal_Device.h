#pragma once
// GPU device abstraction: pipeline state, render pass, stats, program manager, OpenglRenderDevice.

#include "Render/DrawTypedefs.h"
#include "Render/IGraphicsDevice.h"
#include "Framework/Util.h"

#include "glad/glad.h"
#include <string>
#include <vector>

class IGraphicsTexture;
class IGraphicsBuffer;
class Renderer;

// ---------------------------------------------------------------------------
// Render statistics
// ---------------------------------------------------------------------------

struct Render_Stats
{
	int tris_drawn = 0;
	int total_draw_calls = 0;
	int program_changes = 0;
	int texture_binds = 0;
	int vertex_array_changes = 0;
	int blend_changes = 0;
	int framebuffer_changes = 0;
	int framebuffer_clears = 0;

	int shadow_objs = 0;
	int shadow_lights = 0;
};

// ---------------------------------------------------------------------------
// Pipeline state
// ---------------------------------------------------------------------------

struct RenderPipelineState
{
	RenderPipelineState() = default;

	bool backface_culling = true;
	bool cull_front_face = false;
	bool depth_testing = true;
	bool depth_less_than = false;
	bool depth_writes = true;
	BlendState blend = BlendState::OPAQUE;
	// Phase 1.7d: was program_handle (int index into Program_Manager). Now an
	// IGraphicsShader* — set_pipeline can bind without indirection and Phase 2c
	// SDL3 pipeline-cache hashing can read the pointer straight from the struct.
	IGraphicsShader* program = nullptr;
	vertexarrayhandle vao = 0;
};

// ---------------------------------------------------------------------------
// Render pass setup / scope
// ---------------------------------------------------------------------------

struct RenderPassSetup
{
	RenderPassSetup(const char* debug_name, fbohandle framebuffer, bool clear_color, bool clear_depth, int x, int y,
					int w, int h)
		: debug_name(debug_name), framebuffer(framebuffer), clear_color(clear_color), clear_depth(clear_depth), x(x),
		  y(y), w(w), h(h) {}

	const char* debug_name = "";
	fbohandle framebuffer = 0;
	bool clear_color = false;
	bool clear_depth = false;
	float clear_depth_value = 0.0;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct GpuRenderPassScope
{
	GpuRenderPassScope& operator=(const GpuRenderPassScope& other) = delete;
	GpuRenderPassScope(const GpuRenderPassScope& other) = default;

private:
	GpuRenderPassScope(const RenderPassSetup& setup) : setup(setup){};

	const RenderPassSetup setup;
	friend class OpenglRenderDevice;
};

// ---------------------------------------------------------------------------
// Program manager — caches compiled shader programs
// ---------------------------------------------------------------------------

class Program_Manager
{
public:
	program_handle create_single_file(const std::string& shared_file, bool is_tesselation = false,
									  const std::string& defines = {});
	program_handle create_raster(const std::string& frag, const std::string& vert, const std::string& defines = {});
	program_handle create_raster_geo(const std::string& frag, const std::string& vert, const std::string& geo = nullptr,
									 const std::string& defines = {});
	program_handle create_compute(const std::string& compute, const std::string& defines = {});
	IGraphicsShader* get_obj(program_handle handle) const {
		assert(handle >= 0 && handle < programs.size());
		return programs[handle].gfx_shader;
	}
	bool did_shader_fail(program_handle handle) const {
		assert(handle >= 0 && handle < programs.size());
		return programs.at(handle).compile_failed;
	}

	void recompile_all();

	struct program_def
	{
		std::string defines;
		std::string frag;
		std::string vert;
		std::string geo;
		bool is_compute = false;
		bool compile_failed = false;
		bool is_tesselation = false;

		bool is_shared() const { return !vert.empty() && frag.empty() && !is_compute; }
		IGraphicsShader* gfx_shader = nullptr;
	};
	std::vector<program_def> programs;

	void recompile(program_handle handle) {
		if (handle >= 0 && handle < programs.size())
			recompile(programs[handle]);
		else
			sys_print(Warning, "Program_Manager::recompile: handle out of range\n");
	}

private:
	void recompile(program_def& def);
	void recompile_do(program_def& def);
};

// ---------------------------------------------------------------------------
// OpenGL render device
// ---------------------------------------------------------------------------

class OpenglRenderDevice
{
public:
	OpenglRenderDevice() { memset(textures_bound, 0, sizeof(textures_bound)); }
	IGraphicsShader* shader() const { return active_program; }
	Program_Manager& get_prog_man() { return prog_man; }

	void set_pipeline(const RenderPipelineState& pipeline);
	GpuRenderPassScope start_render_pass(const RenderPassSetup& setup);

	void draw_arrays(int mode, int first, int count) {
		activeStats.total_draw_calls++;
		glDrawArrays(mode, first, count);
	}
	void draw_elements_base_vertex(int mode, int count, int type, const void* indicies, int base_vertex) {
		activeStats.total_draw_calls++;
		glDrawElementsBaseVertex(mode, count, type, indicies, base_vertex);
	}
	void multi_draw_elements_indirect(int mode, int type, const void* indirect, int drawcount, int stride) {
		activeStats.total_draw_calls++;
		glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
	}

	const Render_Stats& get_stats() { return lastStats; }

	void reset_states() {
		active_program = nullptr;
		invalidate_all();
	}
	void on_frame_start() {
		lastStats = activeStats;
		activeStats = Render_Stats();
	}

	void set_viewport(int x, int y, int w, int h) { glViewport(x, y, w, h); }
	void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value = 0.f);

	void bind_texture(int bind, int id);
	void bind_texture_ptr(int bind, IGraphicsTexture* ptr) {
		if (ptr)
			bind_texture(bind, ptr->get_internal_handle());
	}
	void set_shader(program_handle handle);
	void set_shader_ptr(IGraphicsShader* shader);
	void set_depth_write_enabled(bool enabled);

private:
	void set_vao(vertexarrayhandle vao);
	void set_blend_state(BlendState blend);
	void set_show_backfaces(bool show_backfaces);
	void set_depth_test_enabled(bool enabled);
	void set_cull_front_face(bool enabled);
	void set_depth_less_than(bool enable_less_than);
	void end_render_pass(GpuRenderPassScope& scope) {
		assert(in_render_pass);
		in_render_pass = false;
	}
	friend struct GpuRenderPassScope;

	static const int MAX_SAMPLER_BINDINGS = 32;
	IGraphicsShader* active_program = nullptr;
	texhandle textures_bound[MAX_SAMPLER_BINDINGS];
	BlendState blending = BlendState::OPAQUE;
	bool show_backface = false;
	bool depth_test_enabled = true;
	bool depth_write_enabled = true;
	bool depth_less_than_enabled = true;
	bool cullfrontface = false;
	fbohandle current_framebuffer = 0;
	vertexarrayhandle current_vao = 0;

	bool in_render_pass = false;

	int framebuffer_changes = 0;
	Render_Stats activeStats;
	Render_Stats lastStats;

	enum invalid_bits
	{
		PROGRAM_BIT,
		BLENDING_BIT,
		BACKFACE_BIT,
		CULLFRONTFACE_BIT,
		DEPTHTEST_BIT,
		DEPTHWRITE_BIT,
		DEPTHLESS_THAN_BIT,
		VAO_BIT,
		FBO_BIT,
		TEXTURE0_BIT,
	};

	bool is_bit_invalid(uint32_t bit) { return invalid_bits & (1ull << bit); }
	void set_bit_valid(uint32_t bit) { invalid_bits &= ~(1ull << bit); }
	void set_bit_invalid(uint32_t bit) { invalid_bits |= (1ull << bit); }
	void invalidate_all() { invalid_bits = UINT64_MAX; }

	Program_Manager prog_man;

private:
	uint64_t invalid_bits = UINT32_MAX;
	friend class Renderer;
};
