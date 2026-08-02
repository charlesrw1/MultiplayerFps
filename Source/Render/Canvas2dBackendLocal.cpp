#include "Render/Canvas2dBackendLocal.h"
#include "Render/Canvas2dGpuMesh.h"
#include "Render/DrawLocal.h"
#include "Render/MaterialLocal.h"
#include "Render/Texture.h"
#include "Render/IGraphicsDevice.h"
#include <algorithm>

namespace {
bool rects_equal(Rect2d a, Rect2d b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
} // namespace

void Canvas2dBackendLocal::render() {
	if (batches.empty())
		return;

	ASSERT(main_window_target);
	gfx().bind_uniform_buffer_base(0, draw.ubo.current_frame);
	auto& device = draw.get_device();

	std::vector<IGraphicsTexture*> already_cleared;

	size_t i = 0;
	while (i < batches.size()) {
		Texture* const target_tex = batches[i].target;
		IGraphicsTexture* const group_target = target_tex ? target_tex->gpu_ptr : main_window_target;
		size_t j = i;
		while (j < batches.size() && batches[j].target == target_tex)
			j++;

		bool clear_color = false;
		Color32 clear_color_val = COLOR_BLACK;
		bool clear_depth = false;
		bool target_already_cleared =
			std::find(already_cleared.begin(), already_cleared.end(), group_target) != already_cleared.end();
		if (!target_already_cleared) {
			for (size_t k = i; k < j; k++) {
				if (batches[k].wants_clear_color) {
					clear_color = true;
					clear_color_val = batches[k].clear_color;
					break;
				}
			}
			for (size_t k = i; k < j; k++) {
				if (batches[k].wants_clear_depth) {
					clear_depth = true;
					break;
				}
			}
			already_cleared.push_back(group_target);
		}

		const bool use_depth = group_target == main_window_target && depth_texture != nullptr;

		RenderPassState pass_state;
		ColorTargetInfo color_info(group_target);
		color_info.wants_clear = clear_color;
		color_info.clear_color =
			glm::vec4(clear_color_val.r / 255.f, clear_color_val.g / 255.f, clear_color_val.b / 255.f, clear_color_val.a / 255.f);
		auto color_infos = {color_info};
		pass_state.color_infos = color_infos;
		if (use_depth) {
			pass_state.depth_info = depth_texture;
			pass_state.wants_depth_clear = clear_depth;
			pass_state.clear_depth_val = 0.f;
		}
		gfx().set_render_pass(pass_state);

		Rect2d last_viewport(-1, -1, -1, -1);
		bool last_scissor_enabled = false;
		Rect2d last_scissor{};
		bool have_last_scissor = false;

		for (size_t k = i; k < j; k++) {
			Canvas2dBatch& b = batches[k];

			if (!rects_equal(b.viewport, last_viewport)) {
				gfx().set_viewport(b.viewport.x, b.viewport.y, b.viewport.w, b.viewport.h);
				last_viewport = b.viewport;
			}
			if (b.scissor_enabled) {
				if (!have_last_scissor || !last_scissor_enabled || !rects_equal(b.scissor, last_scissor)) {
					gfx().set_scissor(b.scissor.x, b.scissor.y, b.scissor.w, b.scissor.h);
				}
				last_scissor_enabled = true;
				last_scissor = b.scissor;
				have_last_scissor = true;
			} else if (!have_last_scissor || last_scissor_enabled) {
				gfx().disable_scissor();
				last_scissor_enabled = false;
				have_last_scissor = true;
			}

			if (!b.source || b.source->num_indices <= 0 || b.index_count <= 0)
				continue;

			const MaterialInstance* mat = b.material;
			ASSERT(mat);

			RenderPipelineState pipe;
			pipe.backface_culling = true;
			pipe.cull_front_face = false;
			pipe.blend = b.blend;
			pipe.depth_testing = b.depth_test && use_depth;
			pipe.depth_writes = b.depth_test && use_depth;
			pipe.vao = b.source->vao;
			pipe.program = draw.get_prog_man().get_obj(matman.get_mat_shader(nullptr, mat, 0));
			device.set_pipeline(pipe);

			gpu::MasterUIVertPushConsts pcv{};
			pcv.UIViewProj = b.view_proj * b.transform;
			gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

			if (b.texture) {
				device.bind_texture(0, b.texture->gpu_ptr);
			} else {
				auto& texs = mat->impl->get_textures();
				for (int t = 0; t < (int)texs.size(); t++)
					device.bind_texture(t, texs[t]->gpu_ptr);
			}

			gfx().draw_elements_base_vertex(GraphicsPrimitiveType::Triangles, b.index_count, VertexInputIndexType::uint32,
											 b.index_start * (int)sizeof(uint32_t), b.base_vertex);
			draw.stats.total_draw_calls++;
		}

		gfx().disable_scissor();
		i = j;
	}

	device.reset_state_cache();
}
