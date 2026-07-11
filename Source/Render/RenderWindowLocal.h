#pragma once
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
		gfx().bind_uniform_buffer_base(0, draw.ubo.current_frame);
		auto& device = draw.get_device();
		for (int i = 0; i < (int)drawCmds.size(); i++) {
			UIDrawCmdUnion& cmd = drawCmds[i];
			switch (cmd.type) {
			case UiDrawCmdType::DrawCall: {
				UiDrawCallCmd& drawCmd = cmd.drawCmd;
				gfx().draw_elements_base_vertex(GraphicsPrimitiveType::Triangles, drawCmd.index_count,
												VertexInputIndexType::uint32, drawCmd.index_start * (int)sizeof(int),
												drawCmd.base_vertex);
				draw.stats.total_draw_calls++;
			} break;
			case UiDrawCmdType::SetScissor: {
				UiSetScissorCmd& r = cmd.scissorCmd;
				gfx().set_scissor(r.x, r.y, r.w, r.h);
			} break;
			case UiDrawCmdType::ClearScissor:
				gfx().disable_scissor();
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
				pipe.program = draw.get_prog_man().get_obj(matman.get_mat_shader(nullptr, mat, 0));
				pipe.vao = mb_draw_data.vao;
				device.set_pipeline(pipe);

				gpu::MasterUIVertPushConsts pcv{};
				pcv.UIViewProj = view_proj;
				gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

				auto& texs = mat->impl->get_textures();
				for (int i = 0; i < (int)texs.size(); i++)
					device.bind_texture(i, texs[i]->gpu_ptr);
			} break;
			case UiDrawCmdType::SetTexture:
				if (cmd.textureCmd.tex)
					device.bind_texture(cmd.textureCmd.binding, cmd.textureCmd.tex->gpu_ptr);
				else
					device.bind_texture(cmd.textureCmd.binding, draw.white_texture);
				break;
			case UiDrawCmdType::SetModelMatrix:
				break;

			default:
				break;
			}
			draw.stats.total_draw_calls++;
		}

		gfx().disable_scissor();

		device.reset_state_cache();
	}

private:
	glm::mat4 view_proj{};
	MeshBuilderDD mb_draw_data;
};