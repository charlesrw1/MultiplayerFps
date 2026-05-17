// Phase 2c: the GL state cache + render-pass + pipeline state previously lived
// on OpenglRenderDevice in this TU. That role is now part of OpenGLDeviceImpl
// (Source/Render/OpenGlDevice.cpp), exposed via the IGraphicsDevice interface.
// Only the Renderer-side forwarders and Program_Manager bodies remain here.

#include "DrawLocal.h"
#include "IGraphicsDevice.h"
#include "Framework/Util.h"

void Renderer::bind_texture(int bind, int id) {
	gfx().bind_texture_unit_raw(bind, (uint32_t)id);
}

void Renderer::bind_vao(uint32_t vao) {
	gfx().set_vao_raw(vao);
}

void Renderer::set_blend_state(BlendState blend) {
	gfx().set_blend_state(blend);
}
void Renderer::set_show_backfaces(bool show_backfaces) {
	gfx().set_show_backfaces(show_backfaces);
}

void Renderer::set_shader(program_handle handle) {
	gfx().set_shader_ptr(handle == -1 ? nullptr : prog_man.get_obj(handle));
}

IGraphicsShader* Renderer::shader() {
	return gfx().get_active_shader();
}

void Renderer::on_frame_start() {
	stats = Render_Stats();
	gfx().reset_state_cache();
}

// ---------------------------------------------------------------------------
// Program_Manager
// ---------------------------------------------------------------------------

extern ConfigVar log_shader_compiles;

program_handle Program_Manager::create_single_file(const std::string& shared_file, bool is_tesseltion,
												   const std::string& defines) {
	program_def def;
	def.vert = shared_file;
	def.frag = "";
	def.defines = defines;
	def.is_compute = false;
	def.is_tesselation = is_tesseltion;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster(const std::string& vert, const std::string& frag,
											  const std::string& defines) {
	program_def def;
	def.vert = vert;
	def.frag = frag;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster_geo(const std::string& vert, const std::string& frag,
												  const std::string& geo, const std::string& defines) {
	program_def def;
	def.vert = vert;
	def.frag = frag;
	def.geo = geo;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_compute(const std::string& compute, const std::string& defines) {
	program_def def;
	def.vert = compute;
	def.defines = defines;
	def.is_compute = true;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
void Program_Manager::recompile_all() {
	for (int i = 0; i < programs.size(); i++)
		recompile(programs[i]);
}
void Program_Manager::recompile(program_def& def) {
	double start = GetTime();
	recompile_do(def);
	float time = GetTime() - start;
	if (log_shader_compiles.get_bool())
		sys_print(Debug, "Program_Manager::recompile: compiled/loaded %s in %f\n", def.vert.c_str(), time);
}

void Program_Manager::recompile_do(program_def& def) {
	safe_release(def.gfx_shader);

	if (def.is_compute)
		def.gfx_shader = gfx().create_shader_compute(def.vert, def.defines);
	else if (def.is_shared() && def.is_tesselation)
		def.gfx_shader = gfx().create_shader_single_file_tess(def.vert, def.defines);
	else if (def.is_shared())
		def.gfx_shader = gfx().create_shader_single_file(def.vert, def.defines);
	else if (!def.geo.empty())
		def.gfx_shader = gfx().create_shader_vert_frag_geo(def.vert, def.frag, def.geo, def.defines);
	else
		def.gfx_shader = gfx().create_shader_vert_frag(def.vert, def.frag, def.defines);

	def.compile_failed = (def.gfx_shader == nullptr);
}
