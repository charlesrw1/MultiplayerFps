#include "DrawLocal.h"
#include "Framework/Util.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL3/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

namespace {
// gpu::BloomParams is declared in Shaders/ShaderBufferShared.txt and shared
// verbatim with the BloomDownsampleF/BloomUpsampleF GLSL.
static_assert(sizeof(gpu::BloomParams) == 16, "std140");
static_assert(sizeof(gpu::AutoExposureParams) == 48, "std140");

// Per-pass UBO group slot. See [[rendering/gfx_abstraction#2a]].
constexpr int PER_PASS_PARAMS_UBO_BINDING = 7;
// Auto-exposure UBO sits at slot 8 (non-conflicting).
constexpr int AE_PARAMS_UBO_BINDING = 8;
}

void Renderer::render_bloom_chain(IGraphicsTexture* scene_color) {
	ZoneScoped;
	GPUFUNCTIONSTART;

	if (!enable_bloom.get_bool())
		return;

	// gfx().reset_state_cache();

	//	RenderPassSetup setup("bloompass", fbo.bloom, false, false, 0, 0, cur_w, cur_h);
	//	auto scope = device.start_render_pass(setup);

	/// IGraphicsDevice* device = (&gfx());

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = draw.get_prog_man().get_obj(prog.bloom_downsample);
		gfx().set_pipeline(state);

		//*set_shader(prog.bloom_downsample);
		float src_x = cur_w;
		float src_y = cur_h;

		gfx().bind_texture(0, scene_color);
		for (int i = 0; i < tex.number_bloom_mips; i++) {
			auto& bc = tex.bloom_chain[i];

			// glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0,bc.texture->get_internal_handle(), 0);

			auto setup_pass = [&]() {
				ColorTargetInfo target(bc.texture);
				target.wants_clear = true; // clear to black (default)
				auto color_infos = {target};
				RenderPassState pass;
				pass.color_infos = color_infos;
				gfx().set_render_pass(pass);
			};
			setup_pass();

			gpu::BloomParams params{};
			params.srcResolution = glm::vec2(src_x, src_y);
			params.mipLevel = i;
			ubo.bloom_params->upload(&params, sizeof(params));
			gfx().bind_uniform_buffer_base(PER_PASS_PARAMS_UBO_BINDING, ubo.bloom_params);
			src_x = bc.fsize.x;
			src_y = bc.fsize.y;

			gfx().set_viewport(0, 0, src_x, src_y);
			gfx().clear_framebuffer(false, true /* clear color*/);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

			// glBindTextureUnit(0, bc.texture->get_internal_handle());
			gfx().bind_texture(0, bc.texture);
		}
	}

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = draw.get_prog_man().get_obj(prog.bloom_upsample);
		state.blend = BlendState::ADD;
		gfx().set_pipeline(state);

		for (int i = tex.number_bloom_mips - 1; i > 0; i--) {
			auto& bc = tex.bloom_chain[i - 1];

			// glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0, bc.texture->get_internal_handle(), 0);
			auto setup_pass = [&]() {
				auto color_infos = {ColorTargetInfo(bc.texture)};
				RenderPassState pass;
				pass.color_infos = color_infos;
				gfx().set_render_pass(pass);
			};
			setup_pass();

			vec2 destsize = bc.fsize;
			gfx().set_viewport(0, 0, destsize.x, destsize.y);

			// glBindTextureUnit(0, bc.texture->get_internal_handle());
			gfx().bind_texture(0, bc.texture);

			gpu::BloomParams params{};
			params.filterRadius = 0.0001f;
			ubo.bloom_params->upload(&params, sizeof(params));
			gfx().bind_uniform_buffer_base(PER_PASS_PARAMS_UBO_BINDING, ubo.bloom_params);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}
	}

	// gfx().reset_state_cache();
}

void Renderer::render_auto_exposure(IGraphicsTexture* scene_hdr, const PostProcessParams& pp, float dt) {
	if (!pp.auto_exposure) return;
	ZoneScoped;
	GPUFUNCTIONSTART;

	// Build and upload AutoExposureParams
	gpu::AutoExposureParams aep{};
	aep.ae_speed_up     = pp.ae_speed_up;
	aep.ae_speed_down   = pp.ae_speed_down;
	aep.ae_key          = pp.ae_key;
	aep.ae_min_ev       = pp.ae_min_ev;
	aep.ae_max_ev       = pp.ae_max_ev;
	aep.ae_dt           = dt;
	aep.ae_hist_min_log = -10.f;
	aep.ae_hist_max_log =  4.f;
	aep.ae_low_pct      = pp.ae_low_pct;
	aep.ae_high_pct     = pp.ae_high_pct;
	buf.ae_params->upload(&aep, sizeof(aep));
	gfx().bind_uniform_buffer_base(AE_PARAMS_UBO_BINDING, buf.ae_params);

	int ping = ae_ping;
	int pong = 1 - ae_ping;

	if (pp.ae_method == 0) {
		// --- Method 0: downsample (read bloom chain tail) ---
		// The smallest bloom mip is already a spatial average of the scene.
		IGraphicsTexture* bloom_avg = (tex.number_bloom_mips > 0)
			? tex.bloom_chain[tex.number_bloom_mips - 1].texture
			: scene_hdr;

		RenderPipelineState ps;
		ps.vao     = get_empty_vao();
		ps.program = draw.get_prog_man().get_obj(prog.ae_downsample);
		gfx().set_pipeline(ps);

		ColorTargetInfo target(tex.ae_lum[pong]);
		auto color_infos = { target };
		RenderPassState pass_state;
		pass_state.color_infos = color_infos;
		gfx().set_render_pass(pass_state);
		gfx().set_viewport(0, 0, 1, 1);

		gfx().bind_texture(0, bloom_avg);
		gfx().bind_texture(1, tex.ae_lum[ping]);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	} else {
		// --- Method 1: histogram ---
		gfx().begin_compute_pass();

		// Clear histogram
		{ RenderPipelineState ps; ps.program = draw.get_prog_man().get_obj(prog.ae_hist_clear); gfx().set_pipeline(ps); }
		gfx().bind_storage_buffer_base(0, buf.ae_histogram);
		gfx().dispatch_compute(1, 1, 1);
		gfx().memory_barrier(BARRIER_SHADER_STORAGE);

		// Build histogram
		{ RenderPipelineState ps; ps.program = draw.get_prog_man().get_obj(prog.ae_hist_build); gfx().set_pipeline(ps); }
		gfx().bind_texture(0, scene_hdr);
		gfx().bind_storage_buffer_base(0, buf.ae_histogram);
		{
			int gx = (cur_w + 15) / 16;
			int gy = (cur_h + 15) / 16;
			gfx().dispatch_compute(gx, gy, 1);
		}
		gfx().memory_barrier(BARRIER_SHADER_STORAGE);

		// Reduce → adapted exposure
		{ RenderPipelineState ps; ps.program = draw.get_prog_man().get_obj(prog.ae_hist_average); gfx().set_pipeline(ps); }
		gfx().bind_storage_buffer_base(0, buf.ae_histogram);
		gfx().bind_image_for_compute(1, tex.ae_lum[ping], 0, -1, GraphicsImageAccess::ReadOnly);
		gfx().bind_image_for_compute(2, tex.ae_lum[pong], 0, -1, GraphicsImageAccess::WriteOnly);
		gfx().dispatch_compute(1, 1, 1);
		// TEXTURE_FETCH barrier: makes imageStore writes visible to subsequent texture() calls
		gfx().memory_barrier(BARRIER_SHADER_IMAGE_ACCESS | BARRIER_TEXTURE_FETCH);
	}

	ae_ping = pong; // swap for next frame
}

void setup_batch(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled, bool force_show_backfaces,
				 bool depth_less_than_op, const int i, const int offset, float poly_offset_factor = 0.f) {
	const auto& batch = pass.batches[i];
	const auto& mesh_batch = pass.mesh_batches[batch.first];

	const MaterialInstance* mat = (MaterialInstance*)mesh_batch.material;
	const draw_call_key batch_key = pass.objects[mesh_batch.first].sort_key;
	const program_handle program = (program_handle)batch_key.shader;
	const BlendState blend = (BlendState)batch_key.blending;
	const bool show_backface = batch_key.backface;
	const uint32_t layer = batch_key.layer;
	const VaoType vaoType = (VaoType)batch_key.vao;
	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(vaoType);

	RenderPipelineState state;
	state.program = draw.get_prog_man().get_obj(program);
	state.vao = vao_ptr;
	state.backface_culling = !show_backface && !force_show_backfaces;
	state.blend = blend;
	state.depth_testing = depth_test_enabled;
	// state.depth_writes = depth_write_enabled;
	state.depth_writes = !mat->get_master_material()->is_translucent();
	state.depth_less_than = depth_less_than_op;
	if (poly_offset_factor != 0.f) {
		state.polygon_offset_enabled = true;
		state.polygon_offset_factor = poly_offset_factor;
		state.polygon_offset_units = 4.f;
	}
	gfx().set_pipeline(state);

	gpu::MasterDeferredPushConsts pc{};
	pc.indirect_material_offset = (uint32_t)offset;
	gfx().push_vertex_constants(0, &pc, sizeof(pc));
	gfx().push_fragment_constants(0, &pc, sizeof(pc));

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		draw.bind_texture_ptr(i, t->gpu_ptr);
	}
}

void draw_model_simple_no_material(Model* model) {
	if (model->get_num_lods() == 0)
		return;
	auto& lod = model->get_lod(0);
	for (int p = 0; p < lod.part_count; p++) {
		auto& part = model->get_part(p);
		gfx().draw_elements_base_vertex(
			GraphicsPrimitiveType::Triangles, part.element_count, MODEL_INDEX_TYPE,
			part.element_offset + model->get_merged_index_ptr(),
			part.base_vertex + model->get_merged_vertex_ofs());
	}
}

ConfigVar use_client_buffer_mdi("use_client_buffer_mdi", "0", CVAR_BOOL, "");

// M0 (DX11/DX12 de-risk): replace MultiDrawElementsIndirect with a CPU loop of
// single draw_elements_indirect calls, args staying GPU-resident. DX11 has no
// MDI equivalent, so this is the shape the DX11/DX12 backends will always use.
ConfigVar r_indirect_loop("r_indirect_loop", "0", CVAR_BOOL, "");
int setup_execute_render_lists(Render_Lists& list, Render_Pass& pass) {
	auto& scene = draw.scene;

	IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();

	gfx().bind_storage_buffer_base(2, scene.gpu_instance_buffer);
	gfx().bind_storage_buffer_base(3, scene.gpu_skinned_mats_buffer);
	gfx().bind_storage_buffer_base(4, material_buffer);
	gfx().bind_storage_buffer_base(5, list.glinstance_to_instance);
	int offset_command_bytes = 0;
	if (0) {
		const int size = pass.mesh_batches.size() * sizeof(int);
		gfx().bind_storage_buffer_range(6, list.gldrawid_to_submesh_material, size, size);
		const int command_size = list.commands.size() * sizeof(gpu::DrawElementsIndirectCommand);
		offset_command_bytes = command_size;
	} else {
		gfx().bind_storage_buffer_base(6, list.gldrawid_to_submesh_material);
	}

	if (scene.has_lightmap && scene.lightmapObj.lightmap_texture) {
		auto texture = scene.lightmapObj.lightmap_texture;
		draw.bind_texture_ptr(20 /* FIXME, defined to be bound at spot 20,*/, texture->gpu_ptr);
	} else {
		draw.bind_texture_ptr(20 /* FIXME, defined to be bound at spot 20,*/, draw.black_texture);
	}

	return offset_command_bytes;
}

void Renderer::execute_render_lists(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled,
									bool force_show_backfaces, bool depth_less_than_op,
									float poly_offset_factor) {
	const int offset_buffer_start = setup_execute_render_lists(list, pass);
	int offset = 0;
	const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
	for (int i = 0; i < pass.batches.size(); i++) {
		const int count = list.command_count[i];
		const int incr = pass.batches[i].count;
		if (count != 0) {

			setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset,
						poly_offset_factor);

			if (0) {
				auto count_buf = list.get_count_buf();
				ASSERT(count_buf);
				gfx().multi_draw_elements_indirect_count(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					list.gpu_command_list, offset_buffer_start + offset * DEIcmdSz,
					count_buf, i * (int)sizeof(uint32),
					count, sizeof(gpu::DrawElementsIndirectCommand));
			} else if (r_indirect_loop.get_bool()) {
				// M0 indirect-loop path (DX11/DX12 shape): count is CPU-known,
				// culled commands have primCount == 0 and are GPU no-ops.
				// glDrawElementsIndirect always reports gl_DrawID == 0, so
				// MasterShader's `indirect_materials[indirect_material_offset
				// + gl_DrawID]` lookup must be re-pushed per command with
				// indirect_material_offset == offset + dc (setup_batch already
				// pushed offset + 0 for dc == 0).
				gpu::MasterDeferredPushConsts pc{};
				for (int dc = 0; dc < count; dc++) {
					pc.indirect_material_offset = (uint32_t)(offset + dc);
					gfx().push_vertex_constants(0, &pc, sizeof(pc));
					gfx().push_fragment_constants(0, &pc, sizeof(pc));

					gfx().draw_elements_indirect(
						GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
						list.gpu_command_list, offset_buffer_start + (offset + dc) * DEIcmdSz);
				}
			} else if (use_client_buffer_mdi.get_bool()) {
				gfx().multi_draw_elements_indirect(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					nullptr, 0, count, sizeof(gpu::DrawElementsIndirectCommand),
					(const void*)(list.commands.data() + offset));
			} else {
				gfx().multi_draw_elements_indirect(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					list.gpu_command_list, offset_buffer_start + offset * DEIcmdSz,
					count, sizeof(gpu::DrawElementsIndirectCommand));
			}
			stats.total_draw_calls++;
		}
		offset += incr;
	}
}

void Renderer::render_lists_old_way(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled,
									bool force_show_backfaces, bool depth_less_than_op,
									float poly_offset_factor) {
	setup_execute_render_lists(list, pass);
	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset,
					poly_offset_factor);

		const int count = list.command_count[i];
		const auto& batch = pass.batches[i];
		for (int dc = 0; dc < batch.count; dc++) {
			auto& cmd = list.commands.at(offset + dc);

			gfx().draw_elements_instanced_base_vertex_base_instance(
				GraphicsPrimitiveType::Triangles, cmd.count, MODEL_INDEX_TYPE,
				cmd.firstIndex * MODEL_BUFFER_INDEX_TYPE_SIZE,
				cmd.primCount, cmd.baseVertex, cmd.baseInstance);

			stats.total_draw_calls++;
		}

		offset += count;
	}
}

void Renderer::render_level_to_target(const Render_Level_Params& params) {
	ZoneScoped;
	// TracyGpuZone("render_to_target");

	gfx().reset_state_cache();

	IGraphicsBuffer* what_ubo = params.provied_constant_buffer;
	{
		bool upload = params.upload_constants;
		if (what_ubo == nullptr) {
			what_ubo = ubo.current_frame;
			upload = true;
		}
		if (upload)
			upload_ubo_view_constants(params.view, what_ubo, params.wireframe_secondpass);
	}

	gfx().bind_uniform_buffer_base(0, what_ubo);

	const float poly_offset_factor =
		(params.pass == Render_Level_Params::SHADOWMAP) ? params.offset_poly_units : 0.f;

	if (params.pass == Render_Level_Params::FORWARD_PASS) {
		// fixme, for lit transparents
		const Texture* reflectionProbeTex = scene.get_reflection_probe_for_render(params.view.origin);
		if (reflectionProbeTex)
			bind_texture_ptr(19, reflectionProbeTex->gpu_ptr);
		else {
			// uh...
			bind_texture_ptr(19, black_texture); // expects a cubemap...
		}
		bind_texture_ptr(18, EnviornmentMapHelper::get().integrator.get_texture());
	}

	if (params.rl && params.rp) {
		// shadows map dont have reversed Z, just standard 0,1 depth
		//*if (params.pass != Render_Level_Params::SHADOWMAP)
		//*	glDepthFunc(GL_GREATER);

		const bool force_backface_state =
			params.pass == Render_Level_Params::SHADOWMAP || r_debug_mode.get_integer() != 0;

		const bool depth_less_than =
			params.wants_non_reverse_z; // params.pass == Render_Level_Params::SHADOWMAP;	// else, GL_GREATER
		const bool depth_testing = true;
		// const bool depth_writes = params.pass != Render_Level_Params::TRANSLUCENT;
		if (dont_use_mdi.get_bool()) {
			render_lists_old_way(*params.rl, *params.rp, depth_testing, force_backface_state, depth_less_than,
								 poly_offset_factor);
		} else {
			execute_render_lists(*params.rl, *params.rp, depth_testing, force_backface_state, depth_less_than,
								 poly_offset_factor);
		}
	}

	// glClearDepth(1.0);
	// glDepthFunc(GL_LESS);
	// Polygon offset reset implicitly: each pipeline state carries its own
	// polygon_offset_* fields, and reset_state_cache below dirties everything.

	gfx().reset_state_cache();
}

void Renderer::render_particles() {
	gfx().reset_state_cache();
	auto& pobjs = scene.particle_objs.objects;
	auto* default_mat = MaterialInstance::load("particle_default.mm");
	assert(default_mat);
	for (auto& p_ : pobjs) {
		auto& p = p_.type_;
		const MaterialInstance* mat = p.obj.material;
		if (!mat)
			mat = default_mat;

		RenderPipelineState state;
		state.program = draw.get_prog_man().get_obj(matman.get_mat_shader(nullptr, mat, 0));
		state.vao = p.dd.vao;
		state.backface_culling = mat->get_master_material()->backface;
		state.blend = mat->get_master_material()->blend;
		state.depth_testing = true;
		state.depth_writes = state.blend == BlendState::OPAQUE;
		state.depth_less_than = false;
		gfx().set_pipeline(state);

		gpu::MasterParticleVertPushConsts pcv{};
		pcv.ViewProj = current_frame_view.viewproj;
		pcv.Model    = p.obj.transform;
		gfx().push_vertex_constants(0, &pcv, sizeof(pcv));
		gpu::MasterParticleFragPushConsts pcf{};
		pcf.FS_IN_Matid = mat->impl->gpu_buffer_offset;
		gfx().push_fragment_constants(0, &pcf, sizeof(pcf));

		auto& textures = mat->impl->get_textures();
		for (int i = 0; i < textures.size(); i++) {
			Texture* tex = textures[i];
			IGraphicsTexture* gfx_tex = white_texture;
			if (tex)
				gfx_tex = tex->gpu_ptr;
			bind_texture_ptr(i, gfx_tex);
		}

		gfx().draw_elements(GraphicsPrimitiveType::Triangles, p.dd.num_indicies,
							VertexInputIndexType::uint32, 0);
	}
}

#include <algorithm>

static void build_standard_cpu(Render_Lists& list, Render_Pass& src, Free_List<ROP_Internal>& proxy_list) {
	ZoneScopedN("build_standard_cpu");

	auto& memArena = draw.get_arena();
	ArenaScope memScope(memArena);
	std::span<uint32_t> draw_to_material = memArena.alloc_bottom_span<uint32_t>(src.mesh_batches.size());

	// first build the lists
	list.build_from(src, proxy_list, draw_to_material);

	const int objCount = src.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = src.objects[objIndex];

		uint32_t precount = list.commands[obj.batch_idx].primCount++; // increment count
		uint32_t ofs = list.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = proxy_list.handle_to_obj[obj.render_obj.id];
	}

	// cull objects (just cull every obj, f it)
	// then dispatch a compute that looks in glinstance_to_instance, finds instance vis status
	// then increments

	list.gldrawid_to_submesh_material->upload(
		draw_to_material.data(), sizeof(uint32_t) * draw_to_material.size());

	list.glinstance_to_instance->upload(
		glinstance_to_instance, sizeof(uint32_t) * objCount);

	const int command_list_size_bytes = sizeof(gpu::DrawElementsIndirectCommand) * list.commands.size();
	list.gpu_command_list->upload(list.commands.data(), command_list_size_bytes);
}
void Render_Lists_Gpu_Culled::init(uint32_t drawidsz, uint32_t instbufsz) {
	Render_Lists::init(drawidsz, instbufsz);
	inst_to_obj = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	count_buffer = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	batches_buf = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
}

ConfigVar collapse_draw_calls("collapse_draw_calls", "1", CVAR_BOOL | CVAR_DEV, "");
static void build_cascade_cpu(Render_Lists& shadowlist, Render_Pass& shadowpass, Free_List<ROP_Internal>& proxy_list,
							  uint8_t* visiblity) {
	Memory_Arena& memArena = draw.get_arena();
	ArenaScope memScope(memArena);
	std::span<uint32_t> draw_to_material = memArena.alloc_bottom_span<uint32_t>(shadowpass.mesh_batches.size());

	// first build the lists
	shadowlist.build_from(shadowpass, proxy_list, draw_to_material);

	const int objCount = shadowpass.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = shadowpass.objects[objIndex];
		int id = proxy_list.handle_to_obj[obj.render_obj.id];
		// ASSERT(visiblity[id]);

		bool visible = true;
		int8_t wantlod = 0;
		// split_input_lod_arr(visiblity[id], visible, wantlod);
		if (!visible)
			continue;

		uint32_t precount = shadowlist.commands[obj.batch_idx].primCount++; // increment count
		uint32_t ofs = shadowlist.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = id;
	}

	auto collapse_commands = [](Render_Lists& list, std::span<uint32_t> draw_to_material) {
		assert(draw_to_material.size() == list.commands.size());
		int command_ofs = 0;
		for (int j = 0; j < list.command_count.size(); j++) {
			const int cmd_cnt = list.command_count[j];
			std::span<gpu::DrawElementsIndirectCommand> sub_span(&list.commands.at(command_ofs), cmd_cnt);
			std::span<uint32_t> sub_drawid_to_mat_span(&draw_to_material[command_ofs], cmd_cnt);
			int new_count = 0;
			for (int i = 0; i < cmd_cnt; i++) {
				if (sub_span[i].primCount != 0) {
					if (new_count != i) {
						sub_span[new_count] = sub_span[i];
						sub_drawid_to_mat_span[new_count] = sub_drawid_to_mat_span[i];
					}
					new_count += 1;
				}
			}
			list.command_count[j] = new_count;

			command_ofs += cmd_cnt;
		}
	};
	// collapses draw calls so 0 instance calls are removed.
	// this only applies to shadows as gbuffer/transparent passes will always have >= 1 instances
	// seems like a small performance win, but not really nessecary
	if (collapse_draw_calls.get_bool())
		collapse_commands(shadowlist, draw_to_material);

	shadowlist.gldrawid_to_submesh_material->upload(
		draw_to_material.data(), sizeof(uint32_t) * draw_to_material.size());

	// Two-step (upload to size, then sub_upload) preserves the original semantics
	// from when this used glNamedBufferData+glNamedBufferSubData. sub_upload is a
	// no-op when objCount==0 (matches the original glNamedBufferSubData size==0
	// path build_cascade_cpu relied on for empty cascades).
	shadowlist.glinstance_to_instance->upload(nullptr, sizeof(uint32_t) * objCount);
	if (objCount > 0) {
		shadowlist.glinstance_to_instance->sub_upload(
			glinstance_to_instance, sizeof(uint32_t) * objCount, 0);
	}

	auto& list = shadowlist;
	const int command_list_size_bytes = sizeof(gpu::DrawElementsIndirectCommand) * list.commands.size();
	list.gpu_command_list->upload(list.commands.data(), command_list_size_bytes);
}
