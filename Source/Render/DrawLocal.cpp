#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>

#include "UI/GUISystemPublic.h" // for GuiSystemPublic::paint
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h" // FIXME
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"

const GLenum MODEL_INDEX_TYPE_GL = GL_UNSIGNED_SHORT;

//#pragma optimize("", off)

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;

Renderer draw;
RendererPublic* idraw = &draw;

// HOLY CONFIG VARS
ConfigVar enable_vsync("r.enable_vsync", "1", CVAR_BOOL, "enable/disable vsync");
ConfigVar shadow_quality_setting("r.shadow_setting", "0", CVAR_INTEGER, "csm shadow quality", 0, 3);
ConfigVar enable_bloom("r.bloom", "1", CVAR_BOOL, "enable/disable bloom");
ConfigVar enable_volumetric_fog("r.vol_fog", "0", CVAR_BOOL, "enable/disable volumetric fog");
ConfigVar enable_ssao("r.ssao", "1", CVAR_BOOL, "enable/disable screen space ambient occlusion");
ConfigVar use_halfres_reflections("r.halfres_reflections", "1", CVAR_BOOL, "");
ConfigVar dont_use_mdi("r.dont_use_mdi", "0", CVAR_BOOL | CVAR_DEV,
					   "disable multidrawindirect and use drawelements instead");
// 12mb arena
ConfigVar renderer_memory_arena_size("r.mem_arena_size", "12000000", CVAR_INTEGER | CVAR_UNBOUNDED,
									 "size of the renderers memory arena in bytes");

ConfigVar r_taa_enabled("r.taa", "1", CVAR_BOOL, "enable temporal anti aliasing");

static const int MAX_TAA_SAMPLES = 16;
ConfigVar r_taa_samples("r.taa_samples", "4", CVAR_INTEGER, "", 2, MAX_TAA_SAMPLES);
ConfigVar r_taa_32f("r.taa_32f", "0", CVAR_BOOL, "use 32 bit scene motion buffer instead of 16 bit");

// basically:
// diffuse_ao = pow(ao, ssao.intensity)
// specular_ao = pow(diffuse_ao,r_specular_ao_intensity)

ConfigVar r_specular_ao_intensity("r.specular_ao_intensity", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar r_debug_skip_build_scene_data("r.debug_skip_build_scene_data", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_gbuffer("r_skip_gbuffer", "0", CVAR_BOOL, "");

ConfigVar force_render_cubemaps("r.force_cubemap_render", "0", CVAR_BOOL | CVAR_DEV,
								"force cubemaps to re-render, treated like a flag and not a setting");

ConfigVar r_drawterrain("r.drawterrain", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of terrain");
ConfigVar r_force_hide_ui("r.force_hide_ui", "0", CVAR_BOOL, "disable ui drawing");
ConfigVar test_thumbnail_model("test_thumbnail_model", "", CVAR_DEV, "");

ConfigVar r_drawdecals("r.drawdecals", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of decals");

ConfigVar thumbnail_fov("thumbnail_fov", "60", CVAR_FLOAT | CVAR_UNBOUNDED, "");

ConfigVar debug_sun_shadow("r.debug_csm", "0", CVAR_BOOL | CVAR_DEV, "debug csm shadow rendering");
ConfigVar debug_specular_reflection("r.debug_specular", "0", CVAR_BOOL | CVAR_DEV, "debug specular lighting");
ConfigVar r_no_indirect("r.no_indirect", "0", CVAR_BOOL, "");

ConfigVar r_no_meshbuilders("r_no_meshbuilders", "0", CVAR_BOOL | CVAR_DEV, "");
// 128 bones * 100 characters * 2 (double bffer) =
ConfigVar r_skinned_mats_bone_buffer_size("r.skinned_mats_bone_buffer_size", "25600",
										  CVAR_INTEGER | CVAR_UNBOUNDED | CVAR_READONLY, "");

ConfigVar r_better_depth_batching("r.better_depth_batching", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_no_batching("r.no_batching", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_ignore_depth_shader("r_ignore_depth_shader", "0", CVAR_BOOL | CVAR_DEV, "");

ConfigVar enable_gl_debug_output("enable_gl_debug_output", "1", CVAR_BOOL, "");
ConfigVar r_taa_jitter_test("r.taa_jitter_test", "0", CVAR_INTEGER, "", 0, 4);
ConfigVar r_normal_shaded_debug("r.normal_shaded_debug", "1", CVAR_BOOL, "");
ConfigVar log_shader_compiles("log_shader_compiles", "0", CVAR_BOOL, "");

ConfigVar r_debug_mode("r.debug_mode", "0", CVAR_INTEGER | CVAR_DEV,
					   "render debug mode, see Draw.cpp for DEBUG_x values, 0 to disable", 0, 200);

ConfigVar r_drawfog("r.drawfog", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of fog");

ConfigVar ddgi_test("dt", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar ddgi_rt("ddrt", "0", CVAR_BOOL | CVAR_DEV, "");

extern void build_frustum_for_cascade(Frustum& f, int index);

RenderWindowBackend* RenderWindowBackend::inst = nullptr;
class RenderWindowBackendLocal : public RenderWindowBackend
{
public:
	int id_counter = 1;

	std::vector<UIDrawCmdUnion> drawCmds;

	handle<RenderWindow> register_window() { return {1}; }
	void update_window(handle<RenderWindow> handle, RenderWindow& data) final {
		assert(handle.id == 1);
		// return;
		drawCmds = data.get_draw_cmds();
		mb_draw_data.init_from(data.meshbuilder);
		this->view_proj = data.view_mat;
	}
	virtual void remove_window(handle<RenderWindow> handle) final { assert(handle.id == 1); }

	void render() {
		// return;
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.ubo.current_frame);
		auto& device = draw.get_device();
		for (int i = 0; i < drawCmds.size(); i++) {
			const GLenum mode = GL_TRIANGLES;

			UIDrawCmdUnion& cmd = drawCmds[i];
			switch (cmd.type) {
			case UiDrawCmdType::DrawCall: {
				UiDrawCallCmd& drawCmd = cmd.drawCmd;
				glDrawElementsBaseVertex(mode, drawCmd.index_count, GL_UNSIGNED_INT,
										 (void*)(drawCmd.index_start * sizeof(int)), drawCmd.base_vertex);
				draw.stats.total_draw_calls++;
			} break;
			case UiDrawCmdType::SetScissor: {
				UiSetScissorCmd& r = cmd.scissorCmd;
				glEnable(GL_SCISSOR_TEST);
				glScissor(r.x, r.y, r.w, r.h);
			} break;
			case UiDrawCmdType::ClearScissor:
				glDisable(GL_SCISSOR_TEST);
				break;
			case UiDrawCmdType::SetPipeline: {
				MaterialInstance* mat = cmd.pipelineCmd.mat;

				assert(mat->get_master_material()->usage == MaterialUsage::UI);
				RenderPipelineState pipe;
				pipe.backface_culling = true;
				pipe.blend = mat->get_master_material()->blend;
				pipe.cull_front_face = false;
				pipe.depth_testing = false;
				pipe.depth_writes = false;
				pipe.program = matman.get_mat_shader(nullptr, mat, 0);
				pipe.vao = mb_draw_data.VAO;
				device.set_pipeline(pipe);

				draw.shader().set_mat4("UIViewProj", view_proj);

				auto& texs = mat->impl->get_textures();
				for (int i = 0; i < texs.size(); i++)
					device.bind_texture_ptr(i, texs[i]->gpu_ptr);
			} break;
			case UiDrawCmdType::SetTexture:
				if (cmd.textureCmd.tex)
					device.bind_texture_ptr(cmd.textureCmd.binding, cmd.textureCmd.tex->gpu_ptr);
				else
					device.bind_texture_ptr(cmd.textureCmd.binding, draw.white_texture);
				break;
			case UiDrawCmdType::SetModelMatrix:
				break;

			default:
				break;
			}
			draw.stats.total_draw_calls++;
		}

		glDisable(GL_SCISSOR_TEST);

		device.reset_states();
	}

private:
	glm::mat4 view_proj{};
	MeshBuilderDD mb_draw_data;
};

class TaaManager
{
public:
	TaaManager() { generateHaltonSequence(MAX_TAA_SAMPLES, jitters); }

	void start_frame() { index = (index + 1) % r_taa_samples.get_integer(); }
	glm::vec2 get_last_frame_jitter(int w, int h) const {
		int previndex = index - 1;
		if (previndex < 0)
			previndex = r_taa_samples.get_integer() - 1;
		return calc_jitter(previndex, w, h);
	}
	glm::vec2 calc_frame_jitter(int width, int height) const { return calc_jitter(index, width, height); }
	glm::mat4 add_jitter_to_projection(const glm::mat4& inproj, glm::vec2 jitter) const {
		glm::mat4 matrix = inproj;
		matrix[2][0] += jitter.x;
		matrix[2][1] += jitter.y;

		return matrix;
	}

private:
	glm::vec2 calc_jitter(int the_index, int width, int height) const {
		auto jit = jitters[the_index]; // [0,1]
		jit = jit - glm::vec2(0.5);	   //[-1/2,1/2]
		return glm::vec2(jit.x / width, jit.y / height);
	}

	static float radicalInverse(int base, int index) {
		float result = 0.0;
		float fraction = 1.0 / base;
		while (index > 0) {
			result += (index % base) * fraction;
			index /= base;
			fraction /= base;
		}
		return result;
	}
	static void generateHaltonSequence(int numPoints, glm::vec2* sequence) {
		const int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
		const int dimension = 2;
		for (int d = 0; d < dimension; ++d) {
			int base = primes[d];
			for (int i = 0; i < numPoints; ++i) {
				sequence[i][d] = radicalInverse(base, i + 1);
			}
		}
	}

	glm::vec2 jitters[MAX_TAA_SAMPLES];
	int index = 0;
};
static TaaManager r_taa_manager;

BuildSceneData_CpuFast* BuildSceneData_CpuFast::inst = nullptr;
inline int next_pow2(uint32_t x) {
	return std::bit_ceil(x);
}
#include "Framework/ArenaStd.h"
void BuildSceneData_CpuFast::rebuild_batches() {
	auto make_batches = [&](std::vector<Multidraw_Batch>& batches, const bool is_depth_pass) {
		batches.clear();

		if (out_cmds.empty())
			return;

		Multidraw_Batch batch;
		batch.first = 0;
		batch.count = 1;

		const Model* batch_model = cmd_to_extra.at(0).model;
		auto batch_sort_key = cmd_to_extra.at(0).key;

		for (int i = 1; i < out_cmds.size(); i++) {

			const Model* this_model = cmd_to_extra.at(i).model;
			auto this_sort_key = cmd_to_extra.at(i).key;

			bool batch_this = false;

			bool same_layer = batch_sort_key.layer == this_sort_key.layer;
			bool same_vao = batch_sort_key.vao == this_sort_key.vao;
			bool same_material = batch_sort_key.texture == this_sort_key.texture;
			bool same_shader = batch_sort_key.shader == this_sort_key.shader;
			bool same_other_state =
				batch_sort_key.blending == this_sort_key.blending && batch_sort_key.backface == this_sort_key.backface;

			if (!is_depth_pass) {
				if (same_vao && same_material && same_other_state && same_shader && same_layer)
					batch_this = true; // can batch with different meshes
				else
					batch_this = false;

			} else { // pass==DEPTH
				// can batch across texture changes as long as its not alpha tested
				if (same_shader && same_vao && same_other_state)
					batch_this = true;
				else
					batch_this = false;
			}

			if (batch_this) {
				batch.count += 1;
			} else {
				batches.push_back(batch);
				batch.count = 1;
				batch.first = i;

				batch_model = this_model;
				batch_sort_key = this_sort_key;
			}
		}

		batches.push_back(batch);
	};

	make_batches(gbuffer_pass.batches, false);
	make_batches(shadow_pass.batches, true);
}
void BuildSceneData_CpuFast::upload_gpu_cmds(int sum_count) {
	const int command_bytes_size = out_cmds.size() * sizeof(gpu::DrawElementsIndirectCommand);
	gpu.cmd_list->upload(nullptr, command_bytes_size * 2);
	gpu.cmd_list->sub_upload(out_cmds.data(), command_bytes_size, 0);

	gpu.glinst_to_inst->upload(nullptr, sum_count * sizeof(int) * 2); // *2 because materials stored with instances
}
void setup_batch2(const MaterialInstance* mat, const int offset, bool is_depth, bool depth_less_than_op,
				  bool force_backface, Model* m, bool overdraw_vis) {
	auto flags = (is_depth) ? MSF_DEPTH_ONLY : 0;
	flags |= MSF_MATERIAL_IN_INSTANCE;

	if (is_depth)
		flags |= MSF_NO_TAA;
	if (r_debug_mode.get_integer() != 0)
		flags |= MSF_DEBUG;
	flags |= MSF_EDITOR_ID;
	if (m->has_bones())
		flags |= MSF_ANIMATED;

	const program_handle program = matman.get_mat_shader(nullptr, mat, flags);
	auto master = mat->get_master_material();
	BlendState blend = master->blend;
	const bool show_backface = master->backface;

	VaoType type = VaoType::Lightmapped;
	if (m->has_bones())
		type = VaoType::Animated;
	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(type);

	bool depth_tests = true;
	if (overdraw_vis) {
		blend = BlendState::ADD;
		depth_tests = false;
	}

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface && !force_backface;
	state.blend = blend;
	state.depth_testing = depth_tests;
	// state.depth_writes = depth_write_enabled;
	state.depth_writes = !master->is_translucent();
	state.depth_less_than = depth_less_than_op;

	draw.get_device().set_pipeline(state);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = 0; // t->gl_id;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}
void BuildSceneData_CpuFast::do_draw_shared(int flags, float poly_factor) {
	if (gpu.num_cullobjs <= 0)
		return;

	if (flags & IS_SHADOW) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(poly_factor, 4 /* this does nothing?*/);
	}

	auto do_draw_internal = [&](std::vector<Multidraw_Batch>& batches, const bool is_depth) {
		bool force_backface = false;
		bool want_less_than = false;

		if (is_depth) {
			force_backface = true;
			want_less_than = bool(flags & DEPTH_LESSTHAN);
		}

		IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();
		auto& scene = draw.scene;
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_instance_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, material_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gpu.glinst_to_inst->get_internal_handle());

		const int size = out_cmds.size() * sizeof(int);
		// glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 6, matindirect->get_internal_handle(), size, size);

		const int command_size = out_cmds.size() * sizeof(gpu::DrawElementsIndirectCommand);
		glBindBuffer(GL_PARAMETER_BUFFER, gpu.gbuffer_count->get_internal_handle());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gpu.cmd_list->get_internal_handle());

		const int offset_buffer_start = command_size;
		int offset = 0;
		const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
		for (int i = 0; i < batches.size(); i++) {
			const int count = batches.at(i).count;
			const int mat_ofs = batches.at(i).first;
			// const int count = 1;// list.command_count[i];
			const int incr = count; // pass.batches[i].count;
			if (count != 0) {

				setup_batch2(cmd_to_extra.at(mat_ofs).material, offset, is_depth, want_less_than, force_backface,
							 cmd_to_extra.at(mat_ofs).model, flags & OVERDRAWVIS);

				const GLenum index_type = MODEL_INDEX_TYPE_GL;

				void* indirect_ptr = nullptr;

				indirect_ptr = (void*)(int64_t(offset_buffer_start + offset * DEIcmdSz));

				glMultiDrawElementsIndirectCount(GL_TRIANGLES, index_type, indirect_ptr, i * sizeof(uint32), count,
												 sizeof(gpu::DrawElementsIndirectCommand));

				draw.stats.total_draw_calls += 1;
			}
			offset += incr;
		}
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	};

	if (flags & IS_SHADOW)
		do_draw_internal(shadow_pass.batches, true);
	else
		do_draw_internal(gbuffer_pass.batches, false);

	glDisable(GL_POLYGON_OFFSET_FILL);
}
void BuildSceneData_CpuFast::do_shadow_draw(float factor, bool less_than) {
	int flags = IS_SHADOW;
	if (less_than)
		flags |= DEPTH_LESSTHAN;
	do_draw_shared(flags, factor);
}
GpuCullInput BuildSceneData_CpuFast::get_cull_input() const {

	GpuCullInput input;
	input.batches_buf = gpu.gbuffer_batches;
	input.cmd_buf = gpu.cmd_list;
	input.count_buf = gpu.gbuffer_count;
	input.draw_to_batch = gpu.gbuffer_draw_to_batch;
	input.glinst_to_inst = gpu.glinst_to_inst;
	input.mod_data = gpu.mod_data_gpu;
	input.num_batches = gbuffer_pass.batches.size();
	input.num_cmds = out_cmds.size();
	input.obj_data_buf = gpu.cullobj_buf;
	input.num_objs = gpu.num_cullobjs;
	return input;
}
GpuCullInput BuildSceneData_CpuFast::get_cull_input_shadow() const {

	GpuCullInput input = get_cull_input();
	input.batches_buf = gpu.shadow_batches;
	input.draw_to_batch = gpu.shadow_draw_to_batch;
	input.num_batches = shadow_pass.batches.size();

	return input;
}
void BuildSceneData_CpuFast::do_gbuffer_draw(bool overdraw_visualization_2nd_pass) {
	int flags = 0;
	if (overdraw_visualization_2nd_pass)
		flags |= OVERDRAWVIS;
	do_draw_shared(flags, 0);
}
void BuildSceneData_CpuFast::rebuild_mod_data() {
	ZoneScopedN("BuildSceneData_CpuFast::rebuild_mod_data");

	ArenaScope scope(draw.mem_arena);

	out_cmds.clear();
	cmd_to_mod_data_ptr.clear();
	cmd_to_extra.clear();

	gbuffer_pass.batches.clear();
	shadow_pass.batches.clear();

	auto make_key = [&](MaterialInstance* this_mat, Model* this_model) -> draw_call_key {
		draw_call_key k{};
		if (!this_mat)
			return k;
		auto parent = this_mat->impl->get_master_impl();
		if (!parent)
			return k;

		k.backface = parent->backface;
		k.blending = (uint64_t)parent->blend;
		k.mesh = this_model->get_uid();
		k.texture = this_mat->impl->get_texture_id_hash();
		int flags = 0;
		if (this_model->has_bones())
			flags |= MSF_ANIMATED;
		k.shader = matman.get_mat_shader(nullptr, this_mat, flags);
		return k;
	};

	arena_vec<int> mod_data_gpu_buf(scope);
	mod_data_gpu_buf.reserve(10'000);

	for (auto& [key, md] : mod_data) {
		auto m = key.m;

		const int bufstart = mod_data_gpu_buf.size();
		md.gpu_buf_ofs = bufstart;

		mod_data_gpu_buf.push_back(m->get_num_lods());
		for (int lodi = 0; lodi < m->get_num_lods(); lodi++) {
			auto& lod = m->get_lod(lodi);
			mod_data_gpu_buf.push_back(lod.part_ofs);
			mod_data_gpu_buf.push_back(lod.part_count);
			const float f = lod.end_percentage;
			mod_data_gpu_buf.push_back(*((int*)&f));
		}

		const int num_parts = key.m->get_num_parts();
		md.part_to_draw_cmd.clear();
		for (int parti = 0; parti < num_parts; parti++) {
			auto& part = key.m->get_part(parti);
			MaterialInstance* mati = (MaterialInstance*)key.m->get_material_for_part(part);
			if (key.has_textures)
				mati = key.has_textures;
			if (!mati || !mati->impl)
				mati = matman.get_fallback();

			gpu::DrawElementsIndirectCommand cmd{};
			cmd.baseVertex = part.base_vertex + m->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + m->get_merged_index_ptr();
			cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

			// Important! Set primCount to 0 because visible instances will increment this
			cmd.primCount = 0; // meshb.count;
			cmd.baseInstance = 0;
			out_cmds.push_back(cmd);
			cmd_to_mod_data_ptr.push_back(md.ptr_ofs);
			cmd_to_extra.push_back({m, mati, parti, make_key(mati, m)});

			const int cmd_index = (int)out_cmds.size() - 1;
			const int data = (mati->impl->gpu_buffer_offset);

			md.part_to_draw_cmd.push_back(cmd_index);
			md.part_to_draw_cmd.push_back(data);

			mod_data_gpu_buf.push_back(cmd_index);
			mod_data_gpu_buf.push_back(data);
		}
	}
	// sort the commands around

	struct IntAndKey
	{

		int i = 0;
		draw_call_key key{};
		int submesh_idx = 0;
	};

	std::vector<IntAndKey> sorted;

	for (int i = 0; i < out_cmds.size(); i++) {
		sorted.push_back({i, cmd_to_extra[i].key, cmd_to_extra[i].submesh});
	}
	const auto& merge_functor = [](const IntAndKey& a, const IntAndKey& b) {
		if (a.key.as_uint64() < b.key.as_uint64())
			return true;
		else if (a.key.as_uint64() == b.key.as_uint64())
			return a.submesh_idx < b.submesh_idx;
		else
			return false;
	};

	std::sort(sorted.begin(), sorted.end(), merge_functor);

	const arena_vec<gpu::DrawElementsIndirectCommand> copied_cmds(out_cmds.begin(), out_cmds.end(), scope);
	for (int i = 0; i < sorted.size(); i++)
		out_cmds[i] = copied_cmds[sorted[i].i];
	const arena_vec<CmdExtraData> copied_extra(cmd_to_extra.begin(), cmd_to_extra.end(), scope);
	for (int i = 0; i < sorted.size(); i++)
		cmd_to_extra[i] = copied_extra[sorted[i].i];
	const arena_vec<int16_t> copied_ptr_i(cmd_to_mod_data_ptr.begin(), cmd_to_mod_data_ptr.end(), scope);
	for (int i = 0; i < sorted.size(); i++)
		cmd_to_mod_data_ptr[i] = copied_ptr_i[sorted[i].i];

	arena_vec<int> inv_sorted(sorted.size(), scope);
	for (int i = 0; i < sorted.size(); i++) {
		inv_sorted[sorted[i].i] = i;
	}

	// must adjust model index
	for (auto& [key, md] : mod_data) {
		const int num_parts = md.part_to_draw_cmd.size() / 2;
		for (int parti = 0; parti < num_parts; parti++) {

			{
				const int index = parti * 2;
				const int cmd_ofs_prev = md.part_to_draw_cmd.at(index);
				const int remapped = inv_sorted.at(cmd_ofs_prev);
				md.part_to_draw_cmd.at(index) = remapped;
			}
			{
				const int index = md.gpu_buf_ofs + 1 + 3 * md.m->get_num_lods() + parti * 2;
				const int cmd_ofs_prev = mod_data_gpu_buf.at(index);
				const int remapped = inv_sorted.at(cmd_ofs_prev);
				mod_data_gpu_buf.at(index) = remapped;
			}
		}
	}

	rebuild_batches();

	// ##############
	// # GPU UPLOAD #
	// ##############
	gpu.mod_data_gpu->upload(mod_data_gpu_buf.data(), mod_data_gpu_buf.size() * sizeof(int));
	gpu.gbuffer_batches->upload(gbuffer_pass.batches.data(), gbuffer_pass.batches.size() * sizeof(Multidraw_Batch));
	gpu.shadow_batches->upload(shadow_pass.batches.data(), shadow_pass.batches.size() * sizeof(Multidraw_Batch));
	gpu.gbuffer_count->upload(nullptr, sizeof(int) * gbuffer_pass.batches.size());
	gpu.shadows_count->upload(nullptr, sizeof(int) * shadow_pass.batches.size());

	// cmd upload is done with instances

	auto& gb = gbuffer_pass.batches;
	auto& sb = shadow_pass.batches;
	std::span<int> draw_to_batch = draw.mem_arena.alloc_bottom_span<int>(out_cmds.size());
	auto set_and_upload = [&](IGraphicsBuffer* buf, const vector<Multidraw_Batch>& mbv) {
		for (int i = 0; i < mbv.size(); i++) {
			auto& b = mbv.at(i);
			for (int c = 0; c < b.count; c++) {
				ASSERT(b.first + c < draw_to_batch.size());
				draw_to_batch[b.first + c] = i;
			}
		}
		buf->upload(draw_to_batch.data(), draw_to_batch.size_bytes());
	};
	set_and_upload(gpu.gbuffer_draw_to_batch, gb);
	set_and_upload(gpu.shadow_draw_to_batch, sb);
}

// wrinkles
// 1. objects with sort layers? do slow path
// 2. scene wide material overrides?
// 3. models with some transparent parts? -> store this on the model. if has transparents, must iterate them. (this set
// is very small)
// 4. integrate occlusion culling.
// 5. unloading

inline void split_input_lod_arr(uint8_t in, bool& is_vis, int8_t& lod) {
	is_vis = bool(in & 1);
	lod = int8_t(in >> 1);
}
inline void pack_input_lod_arr(uint8_t& out, bool is_vis, int8_t lod) {
	out = uint8_t(is_vis) | uint8_t(lod << 1);
}

BuildSceneData_CpuFast::BuildSceneData_CpuFast() {
	gpu.cmd_list = IGraphicsDevice::inst->create_buffer({});
	gpu.cullobj_buf = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_batches = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_count = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_draw_to_batch = IGraphicsDevice::inst->create_buffer({});
	gpu.glinst_to_inst = IGraphicsDevice::inst->create_buffer({});
	gpu.mod_data_gpu = IGraphicsDevice::inst->create_buffer({});
	gpu.shadows_count = IGraphicsDevice::inst->create_buffer({});
	gpu.shadow_batches = IGraphicsDevice::inst->create_buffer({});
	gpu.shadow_draw_to_batch = IGraphicsDevice::inst->create_buffer({});
}

void BuildSceneData_CpuFast::build_scene_data(bool cubemap_view, bool skybox_only) {
	ZoneScopedN("BuildSceneData_CpuFast");
	CPUFUNCTIONSTART;

	// step1:
	// must check if theres new [model,mat_override] in the renderable set
	// must count up [model,mat_override] pairs, only new model if count > thresh
	// if new model?
	//		-> rebuild
	// else if count > alloc'd?
	//		-> just rebuild the count
	// then for instances
	//		add to mmts_instances if in fast path (hasnt updated etc)

	auto& arena = draw.get_arena();
	auto& proxies = draw.scene.proxy_list.objects;

	ArenaScope scope(arena);

	// step 1.1

	auto mmt_counts = arena.alloc_bottom_span<uint16>(mod_data_ptrs.size());
	std::fill(mmt_counts.begin(), mmt_counts.end(), 0);
	for (auto& [_, obj] : proxies) {
		if (obj.fastcpu_index >= 0)
			mmt_counts[obj.fastcpu_index] += 1;
	}

	// step 1.2
	const int thresh = 1; // if more than 2
	bool wants_rebuild_counts = false;
	bool needs_new_model = force_rebuild;
	for (int c = 0; c < mmt_counts.size(); c++) {
		const int count = mmt_counts[c];
		auto ptr = mod_data_ptrs.at(c);
		if (count >= thresh) {
			ptr->instance_count = count;
			if (count > ptr->instance_alloced) {
				// set how many to alloc

				if (ptr->instance_alloced == 0) {
					needs_new_model = true;
				} else
					wants_rebuild_counts = true;

				ptr->instance_alloced = next_pow2(count);
			}

			// #####################
			// # UNLOADING TESTING #
			// #####################
			// possible for model to not be loaded here. ie user caches a model ptr, not in render system.
			// model is unloaded because its not "used", then user tries using the ptr without going through asset
			// system
			if (count > 0 && !ptr->m->get_is_loaded()) {
				sys_print(Debug, "emergency model reload %s\n", ptr->m->get_name().c_str());
				g_assets.reload_sync<Model>(ptr->m);
			}
		}
	}
	force_rebuild = false;

	// needs_new_model = true;

	// step 1.3
	if (needs_new_model) {
		ZoneScopedN("rebuild_model");

		// the expensive step.
		sys_print(Debug, "rebuilding fast path model data\n");
		rebuild_mod_data();
	}

	if (needs_new_model || wants_rebuild_counts) {
		ZoneScopedN("rebuild_counts");

		// sys_print(Debug, "rebuilding fast path inst counts\n");

		int count_sum = 0;
		for (int cmdi = 0; cmdi < out_cmds.size(); cmdi++) {
			auto ptr = mod_data_ptrs.at(cmd_to_mod_data_ptr.at(cmdi));
			auto& cmd = out_cmds[cmdi];
			cmd.baseInstance = count_sum;
			count_sum += ptr->instance_alloced;
		}

		gbuffer_list.glinstances.resize(count_sum);

		upload_gpu_cmds(count_sum);
	}

	// step2:
	// for render lists:
	//		for objects:
	//			inc render list cmds
	//		compact render list commands
	//		upload cmds, instances
	//		draw the commands (noice)

	{
		ZoneScopedN("bsd_fast_step2");

		for (int i = 0; i < out_cmds.size(); i++)
			out_cmds[i].primCount = 0;

		if (skybox_only)
			return; // none pass

		arena_vec<CullObject> cull_obj_gpu_buf(scope);
		cull_obj_gpu_buf.reserve(proxies.size());

		// step 2.1
		int index = -1;
		for (auto& [_, obj] : proxies) {
			index += 1;

			const int fast_idx = obj.fastcpu_index;
			const bool wants_skip = (fast_idx < 0) || (!obj.proxy.visible) || (obj.proxy.is_skybox) ||
									(cubemap_view && obj.proxy.ignore_in_cubemap);

			if (wants_skip)
				continue;
			ModelAndMatTData* ptr = mod_data_ptrs.at(fast_idx);
			if (ptr->instance_alloced > 0) {
				CullObject co;
				co.bounds_sphere = obj.bounding_sphere_and_radius;
				const int mat_ofs = (obj.proxy.mat_override && obj.proxy.mat_override->impl)
										? (obj.proxy.mat_override->impl->gpu_buffer_offset)
										: -1;
				co.model_ofs = glm::ivec4(ptr->gpu_buf_ofs, index, mat_ofs, 0);
				if (obj.proxy.shadow_caster)
					co.model_ofs.w |= 1;

				cull_obj_gpu_buf.push_back(co);

				// if (vis_list[index] && lod_list[index] > 0) {
				//	const int8_t want_lod = lod_list[index];
				//	auto& lod = obj.proxy.model->get_lod(want_lod);
				//	for (int part = 0; part < lod.part_count; part++) {
				//		const int part_i = lod.part_ofs + part;
				//		const int drawcmd_i = ptr->part_to_draw_cmd.at(part_i * 2);
				//		const int prim = out_cmds.at(drawcmd_i).primCount;
				//		const int base = out_cmds.at(drawcmd_i).baseInstance;
				//		out_cmds.at(drawcmd_i).primCount += 1;
				//		gbuffer_list.glinstances.at(base + prim) = index;
				//	}
				//}
			}
		}
		gpu.cullobj_buf->upload(cull_obj_gpu_buf.data(), cull_obj_gpu_buf.size() * sizeof(CullObject));
		gpu.num_cullobjs = cull_obj_gpu_buf.size();

		// step 2.2
		// gbuffer_list.out_cmds.resize(out_cmds.size());
		// for (auto& md : gbuffer_pass.batches) {
		//	int start = 0;
		//	const int count = md.count;
		//	for (int i = 0; i < count; i++) {
		//		auto& cmd = out_cmds.at(md.first + i);
		//		if (cmd.primCount > 0) {
		//			gbuffer_list.out_cmds.at(md.first + start) = cmd;
		//			start += 1;
		//		}
		//	}
		//}
	}

	GpuCullingTest::inst->build_data(get_cull_input());

	// fully gpu?
	//  cmds and batches always uploaded
	//	mod data always uploaded
	//  only upload instances
	//
	//	upload mmts_instances (very small if you only upload changed portion)
	//	cull + lod on gpu. you are already uploading to gpu anyways for occlusion culling. so look into this!
	// would also have to upload remapping table from objid -> inst buffer... unless you change allocation strategy
}
void cull_and_draw_cascade_fucker(int idx) {
	BuildSceneData_CpuFast::inst->cull_and_draw_shadow_cascade(idx);
}
void cull_and_draw_spot(Frustum f) {
	BuildSceneData_CpuFast::inst->cull_and_draw_shadow_spot(f);
}

void BuildSceneData_CpuFast::cull_and_draw_shadow_cascade(int idx) {
	Frustum f;
	build_frustum_for_cascade(f, idx);
	ASSERT(f.is_ortho);
	GpuCullingTest::inst->do_shadow_cull(get_cull_input_shadow(), f);
	do_shadow_draw(1.0, true);
}

void BuildSceneData_CpuFast::cull_and_draw_shadow_spot(const Frustum& f) {
	GpuCullingTest::inst->do_shadow_cull(get_cull_input_shadow(), f);
	do_shadow_draw(-3, false);
}

void BuildSceneData_CpuFast::make_shadow_object_data_threadsafe(std::span<uint8_t> vis, std::span<int> glinst,
																std::span<gpu::DrawElementsIndirectCommand> outcmds,
																std::span<int> mdcounts) const {

	ASSERT(glinst.size() == gbuffer_list.glinstances.size());
	ASSERT(outcmds.size() == out_cmds.size());
	ASSERT(mdcounts.size() == shadow_pass.batches.size());

	for (int i = 0; i < outcmds.size(); i++) {
		outcmds[i] = out_cmds[i];
		outcmds[i].primCount = 0;
	}

	const auto& arena = draw.get_arena();
	const auto& proxies = draw.scene.proxy_list.objects;
	int index = -1;
	for (auto& [_, obj] : proxies) {
		index += 1;

		const int fast_idx = obj.fastcpu_index;
		const bool wants_skip = (fast_idx < 0) || (!obj.proxy.visible) || (!obj.proxy.shadow_caster);

		if (wants_skip)
			continue;
		ModelAndMatTData* ptr = mod_data_ptrs.at(fast_idx);
		if (ptr->instance_alloced > 0) {

			bool visible{};
			int8_t wantlod{};
			split_input_lod_arr(vis[index], visible, wantlod);
			if (visible) {
				auto& lod = obj.proxy.model->get_lod(wantlod);
				for (int part = 0; part < lod.part_count; part++) {
					const int part_i = lod.part_ofs + part;
					const int drawcmd_i = ptr->part_to_draw_cmd.at(part_i * 2);
					const int prim = out_cmds.at(drawcmd_i).primCount;
					const int base = out_cmds.at(drawcmd_i).baseInstance;
					outcmds[drawcmd_i].primCount += 1;
					glinst[base + prim] = index;
				}
			}
		}
	}

	index = -1;
	for (auto& md : shadow_pass.batches) {
		index += 1;
		int start = 0;
		const int count = md.count;
		int actual_count = 0;
		for (int i = 0; i < count; i++) {
			auto& cmd = outcmds[md.first + i];
			if (cmd.primCount > 0) {
				outcmds[md.first + start] = cmd;
				start += 1;
				actual_count += 1;
			}
		}
		mdcounts[index] = actual_count;
	}
}

void BuildSceneData_CpuFast::on_fastpath_material_removed(MaterialInstance* mat) {
	assert(mat->impl->used_in_fastpath_cache);
	sys_print(Warning, "on_fastpath_material_removed\n");

	auto invalidate_these = [&](int fast_index) {
		for (auto& [_, obj] : draw.scene.proxy_list.objects) {
			if (obj.fastcpu_index == fast_index) {
				obj.fastcpu_index = get_index(obj.proxy.model, obj.proxy.mat_override);
			}
		}
	};

	opt<ModelAndMatTextureSet> found_key;
	int fast_index = -1;
	for (auto& [key, data] : mod_data) {
		if (key.has_textures == mat) {
			found_key = key;
			fast_index = data.ptr_ofs;
			break;
		}
	}
	if (found_key.has_value()) {
		mod_data_ptrs.at(fast_index) = nullptr;
		mod_data.erase(found_key.value());
		invalidate_these(fast_index);
	} else {
		sys_print(Warning, "on_fastpath_material_removed: couldn't find material???\n");
	}
}

void BuildSceneData_CpuFast::on_model_removed(Model* m) {
	// Collect all fast-path cache entries keyed on this model (one per material combo).
	std::vector<int> removed_indices;
	std::vector<ModelAndMatTextureSet> keys_to_erase;
	for (auto& [key, data] : mod_data) {
		if (key.m == m) {
			removed_indices.push_back(data.ptr_ofs);
			keys_to_erase.push_back(key);
		}
	}
	if (removed_indices.empty())
		return;

	sys_print(Debug, "BuildSceneData: nulling %d fast-path entries for removed model\n",
	          (int)removed_indices.size());

	for (int idx : removed_indices)
		mod_data_ptrs.at(idx) = nullptr;
	for (const auto& key : keys_to_erase)
		mod_data.erase(key);

	// Invalidate all proxy objects that referenced any of those cache slots.
	// Set to -1 (not in fast path) rather than re-resolving — the model is gone.
	auto& proxies = draw.scene.proxy_list.objects;
	for (auto& [_, obj] : proxies) {
		for (int idx : removed_indices) {
			if (obj.fastcpu_index == idx) {
				sys_print(Warning, "BuildSceneData: proxy still referencing removed model '%s', nulling fastcpu_index\n",
				          m->get_name().c_str());
				obj.fastcpu_index = -1;
				break;
			}
		}
	}
}