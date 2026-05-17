#pragma once
// Renderer-side support: per-frame stats + Program_Manager. The GL state
// cache, render-pass setup, and pipeline-state struct moved into the OpenGL
// backend (Source/Render/OpenGl*) in Phase 2c — DrawLocal_Device used to hold
// the OpenglRenderDevice class; that role now belongs to OpenGLDeviceImpl.

#include "Render/DrawTypedefs.h"
#include "Render/IGraphicsDevice.h"
#include "Framework/Util.h"

#include <string>
#include <vector>

class IGraphicsShader;

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
