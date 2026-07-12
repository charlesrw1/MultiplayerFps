// DX11 backend skeleton (P3.1 D1). Boots a D3D11 device + swapchain and can
// clear the backbuffer; everything else is an ASSERT(false) stub filled in by
// later sub-phases (D2 resources, D3 pipeline/draws, D4 origin audit, D5
// imgui/tail). See docs/rendering/gfx_abstraction_nextsteps.md.

#include "Dx11Local.h"
#include "Framework/Config.h"
#include "Framework/Util.h"
#include "Render/SpirvCompile.h"

// Declared in DrawLocal_RenderPass.cpp / DrawLocal_Helpers.h. Avoid including
// that header here - it pulls in the GL-side render scene headers, which
// conflict with <d3d11_1.h>'s windows.h macros.
extern ConfigVar r_indirect_loop;

#include <SDL3/SDL.h>

#include "imgui_impl_sdl3.h"
#include "imgui_impl_dx11.h"

#include <algorithm>
#include <climits>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Defined here, used by every Dx11*Impl.cpp factory (see Dx11Local.h).
Microsoft::WRL::ComPtr<ID3D11Device> g_dx11_device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_dx11_context;

namespace {

#define DX11_STUB ASSERT(false && "Dx11: not yet implemented")

UINT dx11_index_stride(VertexInputIndexType t) {
	return t == VertexInputIndexType::uint16 ? 2 : 4;
}

class Dx11DeviceImpl : public IGraphicsDevice
{
public:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain;
	Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation;
	Dx11Texture* backbuffer = nullptr;
	SDL_Window* window = nullptr;

	enum class PassMode { None, Render, Compute };
	PassMode current_pass = PassMode::None;

	Dx11StateCache state_cache;
	RenderPipelineState current_pipeline;
	IGraphicsShader* current_shader = nullptr;
	Dx11VertexInput* current_vao = nullptr;
	GraphicsFillMode current_fill_mode = GraphicsFillMode::Fill;
	D3D11_VIEWPORT current_viewport{};
	// Height of the currently-bound render target(s), used to flip the
	// bottom-left-origin (x, y) passed to set_viewport/set_scissor (GL
	// convention, see IGraphicsDevice.h) into DX11's top-left-origin space.
	int current_rt_height = 0;

	ID3D11RenderTargetView* current_rtvs[RenderPipelineState::MAX_COLOR_ATTACHMENTS]{};
	int num_current_rtvs = 0;
	ID3D11DepthStencilView* current_dsv = nullptr;

	// ---- Bind tables (D3) ---------------------------------------------------
	// Slot indices are SPIR-V/GLSL binding numbers; flush_*_bindings() maps
	// each shader's resource-binding table (spirv_binding -> register_index)
	// onto the actual HLSL register at draw/dispatch time. 32 covers the
	// engine's data slots (0-11) plus the push-constant range
	// (kGfxPushConstBindingBase=12 .. +2*4+3=23).
	static constexpr int MAX_BIND_SLOTS = 32;
	ID3D11ShaderResourceView* bound_srv[MAX_BIND_SLOTS]{};
	ID3D11UnorderedAccessView* bound_uav[MAX_BIND_SLOTS]{};
	ID3D11SamplerState* bound_sampler[MAX_BIND_SLOTS]{};
	ID3D11Buffer* bound_cbuf[MAX_BIND_SLOTS]{};
	Microsoft::WRL::ComPtr<ID3D11Buffer> push_const_bufs[3][IGraphicsDevice::kGfxMaxPushConstSlotsPerStage];

	~Dx11DeviceImpl() override {
		if (backbuffer) {
			backbuffer->release();
			backbuffer = nullptr;
		}
	}

	GraphicsDeviceType get_device_type() override { return GraphicsDeviceType::Dx11; }

	void shutdown_backend() override { spirv_compile_shutdown(); }

	void create_backbuffer_rtv() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_tex;
		HRESULT hr = swapchain->GetBuffer(0, IID_PPV_ARGS(back_tex.GetAddressOf()));
		ASSERT(SUCCEEDED(hr) && "Dx11: GetBuffer(0) failed");

		D3D11_TEXTURE2D_DESC desc{};
		back_tex->GetDesc(&desc);

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		hr = device->CreateRenderTargetView(back_tex.Get(), nullptr, rtv.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: CreateRenderTargetView(backbuffer) failed");

		if (!backbuffer)
			backbuffer = new Dx11Texture();
		backbuffer->rtv = rtv;
		backbuffer->resource = back_tex;
		backbuffer->width = (int)desc.Width;
		backbuffer->height = (int)desc.Height;
		backbuffer->mips = 1;
	}

	// ---- Frame lifecycle ---------------------------------------------------
	void begin_frame() override {}
	IGraphicsTexture* acquire_swapchain_texture() override {
		ASSERT(backbuffer != nullptr);
		return backbuffer;
	}
	void submit_and_present() override {
		swapchain->Present(vsync_enabled ? 1 : 0, 0);
	}

	// ---- Render pass / clear (D2/D3 scope) ----------------------------------
	void set_render_pass(const RenderPassState& state) override {
		ASSERT(state.color_infos.size() <= RenderPipelineState::MAX_COLOR_ATTACHMENTS);
		current_pass = PassMode::Render;

		ID3D11RenderTargetView* rtvs[RenderPipelineState::MAX_COLOR_ATTACHMENTS] = {};
		const int n = (int)state.color_infos.size();
		int min_w = INT_MAX, min_h = INT_MAX;
		for (int i = 0; i < n; i++) {
			const ColorTargetInfo& info = state.color_infos[i];
			Dx11Texture* tex = (Dx11Texture*)info.texture;
			rtvs[i] = (tex == backbuffer) ? backbuffer->rtv.Get() : tex->get_rtv(info.mip, info.layer);
			glm::ivec2 sz = tex->get_size();
			min_w = std::min(min_w, sz.x);
			min_h = std::min(min_h, sz.y);
			if (info.wants_clear) {
				const float c[4] = { info.clear_color.r, info.clear_color.g, info.clear_color.b, info.clear_color.a };
				context->ClearRenderTargetView(rtvs[i], c);
			}
		}

		ID3D11DepthStencilView* dsv = nullptr;
		if (state.depth_info) {
			Dx11Texture* dtex = (Dx11Texture*)state.depth_info;
			dsv = dtex->get_dsv(0, state.depth_layer);
			glm::ivec2 sz = dtex->get_size();
			min_w = std::min(min_w, sz.x);
			min_h = std::min(min_h, sz.y);
			if (state.wants_depth_clear) {
				set_depth_write_enabled(true);
				context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, state.clear_depth_val, 0);
			}
		}

		context->OMSetRenderTargets(n, n > 0 ? rtvs : nullptr, dsv);
		std::memcpy(current_rtvs, rtvs, sizeof(rtvs));
		num_current_rtvs = n;
		current_dsv = dsv;

		if (min_w != INT_MAX) {
			current_rt_height = min_h;
			set_viewport(0, 0, min_w, min_h);
		}
	}

	// x/y/w/h are in GL convention (origin bottom-left of the render target,
	// see IGraphicsDevice.h); RSSetViewports is top-left-origin, so flip y
	// against current_rt_height (set by set_render_pass).
	void set_viewport(int x, int y, int w, int h) override {
		current_viewport.TopLeftX = (float)x;
		current_viewport.TopLeftY = (float)(current_rt_height - (y + h));
		current_viewport.Width = (float)w;
		current_viewport.Height = (float)h;
		current_viewport.MinDepth = 0.f;
		current_viewport.MaxDepth = 1.f;
		context->RSSetViewports(1, &current_viewport);
	}

	void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value) override {
		if (!clear_depth && !clear_color)
			return;
		if (clear_color) {
			const float c[4] = { 0, 0, 0, 1 };
			for (int i = 0; i < num_current_rtvs; i++)
				if (current_rtvs[i])
					context->ClearRenderTargetView(current_rtvs[i], c);
		}
		if (clear_depth && current_dsv) {
			set_depth_write_enabled(true);
			context->ClearDepthStencilView(current_dsv, D3D11_CLEAR_DEPTH, depth_value, 0);
		}
	}

	void blit_textures(const GraphicsBlitInfo& info) override {
		ASSERT(info.src.texture && info.dest.texture);
		ASSERT(info.src.w == info.dest.w && info.src.h == info.dest.h &&
			   "Dx11: scaled blit (different src/dest sizes) deferred to D5");
		Dx11Texture* src = (Dx11Texture*)info.src.texture;
		Dx11Texture* dst = (Dx11Texture*)info.dest.texture;
		UINT src_sub = D3D11CalcSubresource(info.src.mip, std::max(info.src.layer, 0), src->mips);
		UINT dst_sub = D3D11CalcSubresource(info.dest.mip, std::max(info.dest.layer, 0), dst->mips);
		D3D11_BOX box{};
		box.left = (UINT)info.src.x;
		box.right = (UINT)(info.src.x + info.src.w);
		box.top = (UINT)info.src.y;
		box.bottom = (UINT)(info.src.y + info.src.h);
		box.front = 0;
		box.back = 1;
		context->CopySubresourceRegion(dst->resource.Get(), dst_sub, info.dest.x, info.dest.y, 0,
										src->resource.Get(), src_sub, &box);
	}

	// ---- Pipeline state (D3) ------------------------------------------------
	void set_pipeline(const RenderPipelineState& s) override {
		current_pipeline = s;
		current_shader = s.program;
		Dx11ShaderImpl* shader = (Dx11ShaderImpl*)s.program;
		ASSERT(shader != nullptr);

		if (shader->cs) {
			context->CSSetShader(shader->cs.Get(), nullptr, 0);
			return;
		}

		ASSERT(shader->vs && shader->ps);
		context->VSSetShader(shader->vs.Get(), nullptr, 0);
		context->PSSetShader(shader->ps.Get(), nullptr, 0);

		ID3D11RasterizerState* rs = state_cache.get_rasterizer_state(
			s.backface_culling, s.cull_front_face, current_fill_mode,
			s.polygon_offset_enabled, s.polygon_offset_factor, s.polygon_offset_units);
		context->RSSetState(rs);

		ID3D11DepthStencilState* ds = state_cache.get_depth_stencil_state(s.depth_testing, s.depth_writes, s.depth_less_than);
		context->OMSetDepthStencilState(ds, 0);

		ID3D11BlendState* bs = state_cache.get_blend_state(s.blend, s.color_write_masks);
		const float blend_factor[4] = { 0, 0, 0, 0 };
		context->OMSetBlendState(bs, blend_factor, 0xFFFFFFFFu);

		if (s.vao) {
			Dx11VertexInput* vao = (Dx11VertexInput*)s.vao;
			ID3D11InputLayout* layout = state_cache.get_input_layout(vao, shader);
			context->IASetInputLayout(layout);
			ID3D11Buffer* vb = vao->vertex_buffer.Get();
			UINT stride = vao->vertex_stride;
			UINT offset = 0;
			context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
			if (vao->index_buffer)
				context->IASetIndexBuffer(vao->index_buffer.Get(), dx11_index_format(vao->index_type), 0);
			current_vao = vao;
		}
	}

	void set_depth_write_enabled(bool enabled) override {
		current_pipeline.depth_writes = enabled;
		ID3D11DepthStencilState* ds = state_cache.get_depth_stencil_state(
			current_pipeline.depth_testing, enabled, current_pipeline.depth_less_than);
		context->OMSetDepthStencilState(ds, 0);
	}

	IGraphicsShader* get_active_shader() override { return current_shader; }

	void reset_state_cache() override {
		current_shader = nullptr;
		current_vao = nullptr;
	}

	// ---- Bind-table flush (D3) -----------------------------------------------
	// Maps each spirv_binding -> register_index for the given stage's bindings,
	// reading from the bound_* tables populated by bind_texture/bind_sampler/
	// bind_uniform_buffer_base/bind_storage_buffer_*/push_constants_internal.
	void flush_stage(const std::vector<HlslResourceBinding>& bindings,
					 void (ID3D11DeviceContext::*set_srv)(UINT, UINT, ID3D11ShaderResourceView* const*),
					 void (ID3D11DeviceContext::*set_samp)(UINT, UINT, ID3D11SamplerState* const*),
					 void (ID3D11DeviceContext::*set_cb)(UINT, UINT, ID3D11Buffer* const*)) {
		for (auto& b : bindings) {
			ASSERT(b.spirv_binding >= 0 && b.spirv_binding < MAX_BIND_SLOTS);
			switch (b.kind) {
			case HlslRegisterKind::CBV: {
				ID3D11Buffer* buf = bound_cbuf[b.spirv_binding];
				(context.Get()->*set_cb)((UINT)b.register_index, 1, &buf);
				break;
			}
			case HlslRegisterKind::SRV: {
				ID3D11ShaderResourceView* srv = bound_srv[b.spirv_binding];
				(context.Get()->*set_srv)((UINT)b.register_index, 1, &srv);
				break;
			}
			case HlslRegisterKind::Sampler: {
				ID3D11SamplerState* samp = bound_sampler[b.spirv_binding];
				(context.Get()->*set_samp)((UINT)b.register_index, 1, &samp);
				break;
			}
			case HlslRegisterKind::UAV:
				break; // handled separately for compute (CSSetUnorderedAccessViews)
			}
		}
	}

	void flush_render_bindings(Dx11ShaderImpl* shader) {
		flush_stage(shader->vs_bindings, &ID3D11DeviceContext::VSSetShaderResources,
					&ID3D11DeviceContext::VSSetSamplers, &ID3D11DeviceContext::VSSetConstantBuffers);
		flush_stage(shader->ps_bindings, &ID3D11DeviceContext::PSSetShaderResources,
					&ID3D11DeviceContext::PSSetSamplers, &ID3D11DeviceContext::PSSetConstantBuffers);
	}

	void flush_compute_bindings(Dx11ShaderImpl* shader) {
		flush_stage(shader->cs_bindings, &ID3D11DeviceContext::CSSetShaderResources,
					&ID3D11DeviceContext::CSSetSamplers, &ID3D11DeviceContext::CSSetConstantBuffers);
		for (auto& b : shader->cs_bindings) {
			if (b.kind == HlslRegisterKind::UAV) {
				ASSERT(b.spirv_binding >= 0 && b.spirv_binding < MAX_BIND_SLOTS);
				ID3D11UnorderedAccessView* uav = bound_uav[b.spirv_binding];
				UINT initial = (UINT)-1;
				context->CSSetUnorderedAccessViews((UINT)b.register_index, 1, &uav, &initial);
			}
		}
	}

	// ---- Resources (D2) ------------------------------------------------------
	IGraphicsTexture* create_texture(const CreateTextureArgs& args) override { return dx11_create_texture(args); }
	IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) override { return dx11_create_buffer(args); }
	IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) override { return dx11_create_vertex_input(args); }
	IGraphicsSampler* create_sampler(const CreateSamplerArgs& args) override { return dx11_create_sampler(args); }

	// ---- Scissor / draws (D3) ------------------------------------------------
	// x/y/w/h are in GL convention (origin bottom-left, see IGraphicsDevice.h);
	// RSSetScissorRects is top-left-origin, so flip y against current_rt_height
	// (set by set_render_pass), matching set_viewport.
	void set_scissor(int x, int y, int w, int h) override {
		ASSERT(w >= 0 && h >= 0);
		D3D11_RECT rect{ x, current_rt_height - (y + h), x + w, current_rt_height - y };
		context->RSSetScissorRects(1, &rect);
	}
	void disable_scissor() override {
		D3D11_RECT rect{ (LONG)current_viewport.TopLeftX, (LONG)current_viewport.TopLeftY,
						 (LONG)(current_viewport.TopLeftX + current_viewport.Width),
						 (LONG)(current_viewport.TopLeftY + current_viewport.Height) };
		context->RSSetScissorRects(1, &rect);
	}

	void draw_elements_base_vertex(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset, int base_vertex) override {
		ASSERT(count >= 0 && byte_offset >= 0 && current_shader);
		ASSERT((byte_offset % dx11_index_stride(index_type)) == 0);
		flush_render_bindings((Dx11ShaderImpl*)current_shader);
		context->IASetPrimitiveTopology(dx11_primitive_topology(mode));
		context->DrawIndexed((UINT)count, (UINT)(byte_offset / dx11_index_stride(index_type)), base_vertex);
	}
	void draw_arrays(GraphicsPrimitiveType mode, int first, int count) override {
		ASSERT(first >= 0 && count >= 0 && current_shader);
		flush_render_bindings((Dx11ShaderImpl*)current_shader);
		context->IASetPrimitiveTopology(dx11_primitive_topology(mode));
		context->Draw((UINT)count, (UINT)first);
	}
	void draw_elements(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset) override {
		draw_elements_base_vertex(mode, count, index_type, byte_offset, 0);
	}
	void draw_elements_indirect(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int byte_offset) override {
		ASSERT(indirect != nullptr && byte_offset >= 0 && current_shader);
		flush_render_bindings((Dx11ShaderImpl*)current_shader);
		context->IASetPrimitiveTopology(dx11_primitive_topology(mode));
		context->DrawIndexedInstancedIndirect(((Dx11Buffer*)indirect)->buf.Get(), (UINT)byte_offset);
	}
	void draw_elements_instanced_base_vertex_base_instance(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset, int instance_count, int base_vertex, uint32_t base_instance) override {
		ASSERT(count >= 0 && byte_offset >= 0 && instance_count >= 0 && current_shader);
		ASSERT((byte_offset % dx11_index_stride(index_type)) == 0);
		flush_render_bindings((Dx11ShaderImpl*)current_shader);
		context->IASetPrimitiveTopology(dx11_primitive_topology(mode));
		context->DrawIndexedInstanced((UINT)count, (UINT)instance_count,
									   (UINT)(byte_offset / dx11_index_stride(index_type)), base_vertex, base_instance);
	}
	// CPU loop over draw_elements_indirect (M0 indirect-loop path; matches the
	// r_indirect_loop=1 GL path that vars.txt already forces). DX11 has no
	// MultiDrawElementsIndirect equivalent.
	void multi_draw_elements_indirect(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int byte_offset, int draw_count, int stride, const void* client_ptr) override {
		ASSERT(indirect != nullptr && "Dx11: client-side MDI fallback not supported (r_indirect_loop required)");
		ASSERT(draw_count >= 0 && stride > 0 && byte_offset >= 0);
		for (int i = 0; i < draw_count; i++)
			draw_elements_indirect(mode, index_type, indirect, byte_offset + i * stride);
	}
	void multi_draw_elements_indirect_count(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int indirect_byte_offset, IGraphicsBuffer* count, int count_byte_offset, int max_draw_count, int stride) override { DX11_STUB; }

	void wait_for_gpu_idle() override {
		context->Flush();
		Microsoft::WRL::ComPtr<ID3D11Query> query;
		D3D11_QUERY_DESC desc{};
		desc.Query = D3D11_QUERY_EVENT;
		HRESULT hr = device->CreateQuery(&desc, query.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: CreateQuery(EVENT) failed");
		context->End(query.Get());
		BOOL data = FALSE;
		while (context->GetData(query.Get(), &data, sizeof(data), 0) != S_OK) {}
	}

	// ---- Binds (D3) -----------------------------------------------------------
	void bind_texture(int slot, IGraphicsTexture* tex) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS);
		bound_srv[slot] = tex ? ((Dx11Texture*)tex)->get_srv() : nullptr;
	}
	void bind_uniform_buffer_base(int slot, IGraphicsBuffer* buf) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS && buf != nullptr);
		bound_cbuf[slot] = ((Dx11Buffer*)buf)->buf.Get();
	}
	void bind_sampler(int slot, IGraphicsSampler* sampler) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS);
		bound_sampler[slot] = sampler ? dx11_sampler_state(sampler) : nullptr;
	}
	void bind_storage_buffer_base(int slot, IGraphicsBuffer* buf) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS && buf != nullptr);
		Dx11Buffer* b = (Dx11Buffer*)buf;
		bound_srv[slot] = b->get_srv();
		bound_uav[slot] = b->get_uav();
	}
	void bind_storage_buffer_range(int slot, IGraphicsBuffer* buf, int offset, int size) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS && buf != nullptr && offset >= 0 && size > 0);
		Dx11Buffer* b = (Dx11Buffer*)buf;
		bound_srv[slot] = b->get_srv_range(offset, size);
		bound_uav[slot] = b->get_uav_range(offset, size);
	}
	void bind_image_for_compute(int slot, IGraphicsTexture* tex, int mip, int layer, GraphicsImageAccess access) override {
		ASSERT(slot >= 0 && slot < MAX_BIND_SLOTS && tex != nullptr && mip >= 0);
		bound_uav[slot] = ((Dx11Texture*)tex)->get_uav(mip, layer);
	}

	// ---- Push constants (D3) ---------------------------------------------------
	// Lazily create one D3D11_USAGE_DYNAMIC cbuffer per (stage, slot), Map/
	// memcpy/Unmap (WRITE_DISCARD) on every call, and bind it into bound_cbuf
	// at the kGfxPushConstBindingBase-derived slot for the next flush.
	void push_constants_internal(int stage_idx, int slot, const void* data, int size) {
		ASSERT(stage_idx >= 0 && stage_idx < 3);
		ASSERT(slot >= 0 && slot < IGraphicsDevice::kGfxMaxPushConstSlotsPerStage);
		ASSERT(data != nullptr && size > 0 && size <= IGraphicsDevice::kGfxPushConstMaxBytes);
		auto& buf = push_const_bufs[stage_idx][slot];
		if (!buf) {
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = IGraphicsDevice::kGfxPushConstMaxBytes;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			HRESULT hr = device->CreateBuffer(&desc, nullptr, buf.GetAddressOf());
			ASSERT(SUCCEEDED(hr) && "Dx11: push-constant CreateBuffer failed");
		}
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HRESULT hr = context->Map(buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		ASSERT(SUCCEEDED(hr) && "Dx11: push-constant Map failed");
		std::memcpy(mapped.pData, data, size);
		context->Unmap(buf.Get(), 0);

		const int binding = IGraphicsDevice::kGfxPushConstBindingBase
						   + stage_idx * IGraphicsDevice::kGfxMaxPushConstSlotsPerStage + slot;
		ASSERT(binding < MAX_BIND_SLOTS);
		bound_cbuf[binding] = buf.Get();
	}
	void push_vertex_constants(int slot, const void* data, int size) override { push_constants_internal(0, slot, data, size); }
	void push_fragment_constants(int slot, const void* data, int size) override { push_constants_internal(1, slot, data, size); }
	void push_compute_constants(int slot, const void* data, int size) override { push_constants_internal(2, slot, data, size); }

	// ---- Compute (D3) -----------------------------------------------------------
	void begin_compute_pass() override { current_pass = PassMode::Compute; }
	void dispatch_compute(int groups_x, int groups_y, int groups_z) override {
		ASSERT(groups_x >= 0 && groups_y >= 0 && groups_z >= 0);
		ASSERT(current_pass == PassMode::Compute && current_shader &&
			   "dispatch_compute outside compute pass — call begin_compute_pass() first");
		flush_compute_bindings((Dx11ShaderImpl*)current_shader);
		context->Dispatch((UINT)groups_x, (UINT)groups_y, (UINT)groups_z);
	}
	// DX11's immediate context tracks UAV/SRV hazards automatically between
	// dispatches/draws; no explicit barrier API exists (unlike GL).
	void memory_barrier(uint32_t bits) override { ASSERT(bits != 0); }

	// ---- Buffer clear/readback (D2) ----------------------------------------------
	void clear_buffer_uint32(IGraphicsBuffer* buf, uint32_t value) override {
		Dx11Buffer* b = (Dx11Buffer*)buf;
		if (b->buffer_size == 0)
			return;
		const UINT values[4] = {value, value, value, value};
		context->ClearUnorderedAccessViewUint(b->get_uav(), values);
	}
	void download_buffer(IGraphicsBuffer* buf, int offset, int size, void* dest) override {
		ASSERT(size >= 0 && offset >= 0);
		Dx11Buffer* b = (Dx11Buffer*)buf;

		D3D11_BUFFER_DESC staging_desc{};
		staging_desc.ByteWidth = (UINT)size;
		staging_desc.Usage = D3D11_USAGE_STAGING;
		staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
		HRESULT hr = device->CreateBuffer(&staging_desc, nullptr, staging.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: download_buffer staging create failed");

		D3D11_BOX box{};
		box.left = (UINT)offset;
		box.right = (UINT)(offset + size);
		box.top = 0; box.bottom = 1;
		box.front = 0; box.back = 1;
		context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, b->buf.Get(), 0, &box);

		D3D11_MAPPED_SUBRESOURCE mapped{};
		hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		ASSERT(SUCCEEDED(hr) && "Dx11: download_buffer Map failed");
		memcpy(dest, mapped.pData, size);
		context->Unmap(staging.Get(), 0);
	}

	// ---- Misc state (D3/D4) -------------------------------------------------------
	// DX11 line rasterization is always 1px (no glLineWidth equivalent); SDL3
	// GPU silently caps too. Accept and ignore.
	void set_line_width(float width) override { ASSERT(width > 0.0f); }

	void set_polygon_fill_mode(GraphicsFillMode mode) override {
		current_fill_mode = mode;
		ID3D11RasterizerState* rs = state_cache.get_rasterizer_state(
			current_pipeline.backface_culling, current_pipeline.cull_front_face, mode,
			current_pipeline.polygon_offset_enabled, current_pipeline.polygon_offset_factor,
			current_pipeline.polygon_offset_units);
		context->RSSetState(rs);
	}

	void copy_texture(IGraphicsTexture* src, int src_mip, int src_layer, IGraphicsTexture* dst, int dst_mip, int dst_layer, int w, int h) override {
		ASSERT(src && dst);
		Dx11Texture* s = (Dx11Texture*)src;
		Dx11Texture* d = (Dx11Texture*)dst;
		UINT src_sub = D3D11CalcSubresource(src_mip, std::max(src_layer, 0), s->mips);
		UINT dst_sub = D3D11CalcSubresource(dst_mip, std::max(dst_layer, 0), d->mips);
		D3D11_BOX box{};
		box.left = 0; box.right = (UINT)w;
		box.top = 0; box.bottom = (UINT)h;
		box.front = 0; box.back = 1;
		context->CopySubresourceRegion(d->resource.Get(), dst_sub, 0, 0, 0, s->resource.Get(), src_sub, &box);
	}

	// ---- Debug groups + timer queries (D5) -----------------------------------------
	void push_debug_group(const char* name) override {
		ASSERT(name);
		wchar_t wname[256];
		int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, (int)(sizeof(wname) / sizeof(wname[0])));
		if (len <= 0)
			return;
		annotation->BeginEvent(wname);
	}
	void pop_debug_group() override {
		annotation->EndEvent();
	}
	IGraphicsTimerQuery* create_timer_query() override;

	// ---- Window / vsync / imgui (D5) -------------------------------------------------
	bool vsync_enabled = false;
	void set_vsync(bool enable) override { vsync_enabled = enable; }
	void imgui_init() override {
		ASSERT(window != nullptr);
		ImGui_ImplSDL3_InitForD3D(window);
		ImGui_ImplDX11_Init(device.Get(), context.Get());
	}
	void imgui_shutdown() override {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplSDL3_Shutdown();
	}
	void imgui_new_frame() override {
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplSDL3_NewFrame();
	}
	void imgui_render_draw_data() override {
		// Mirrors the GL backend: bind the backbuffer so imgui lands on the
		// swapchain regardless of which offscreen target the last scene pass
		// left bound.
		ID3D11RenderTargetView* rtv = backbuffer->rtv.Get();
		context->OMSetRenderTargets(1, &rtv, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	bool imgui_process_event(const SDL_Event* event) override {
		ASSERT(event != nullptr);
		return ImGui_ImplSDL3_ProcessEvent(event);
	}

	// RmlUi is OpenGL-only for now. These are called unconditionally every
	// frame regardless of backend, so they log-once and no-op rather than
	// using DX11_STUB (which asserts) - there is no unimplemented-path bug
	// here, just no DX11 renderer yet.
	void rmlui_init() override {
		sys_print(Warning, "RmlUi not supported on DX11 backend\n");
	}
	void rmlui_shutdown() override {}
	void rmlui_render(int viewport_w, int viewport_h, IGraphicsTexture* target) override {}

	// ---- Shader factory (D2) -----------------------------------------------------------
	IGraphicsShader* create_shader_vert_frag(const std::string& vert_path, const std::string& frag_path, const std::string& defines) override { return dx11_create_shader_vert_frag(vert_path, frag_path, defines); }
	IGraphicsShader* create_shader_vert_frag_geo(const std::string& vert_path, const std::string& frag_path, const std::string& geo_path, const std::string& defines) override { DX11_STUB; return nullptr; }
	IGraphicsShader* create_shader_compute(const std::string& compute_path, const std::string& defines) override { return dx11_create_shader_compute(compute_path, defines); }
	IGraphicsShader* create_shader_single_file(const std::string& shared_path, const std::string& defines) override { return dx11_create_shader_single_file(shared_path, defines); }
	IGraphicsShader* create_shader_single_file_tess(const std::string& shared_path, const std::string& defines) override { DX11_STUB; return nullptr; }
};

// ID3D11Query TIMESTAMP wrapper. DX11 timestamps are only meaningful relative
// to a TIMESTAMP_DISJOINT query's frequency/disjoint flag, so each instance
// brackets its own timestamp with a disjoint query (one extra query per
// record_timestamp(), but keeps the IGraphicsTimerQuery interface
// self-contained, matching OpenGLTimerQueryImpl's single-call model).
class Dx11TimerQueryImpl : public IGraphicsTimerQuery
{
public:
	Dx11TimerQueryImpl() {
		D3D11_QUERY_DESC ts_desc{};
		ts_desc.Query = D3D11_QUERY_TIMESTAMP;
		g_dx11_device->CreateQuery(&ts_desc, timestamp.GetAddressOf());

		D3D11_QUERY_DESC disjoint_desc{};
		disjoint_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		g_dx11_device->CreateQuery(&disjoint_desc, disjoint.GetAddressOf());
	}
	void release() override { delete this; }

	void record_timestamp() override {
		g_dx11_context->Begin(disjoint.Get());
		g_dx11_context->End(timestamp.Get());
		g_dx11_context->End(disjoint.Get());
		armed = true;
	}

	bool is_available() override {
		if (!armed) return false;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data;
		return g_dx11_context->GetData(disjoint.Get(), &data, sizeof(data), 0) == S_OK;
	}

	uint64_t read_timestamp_ns() override {
		ASSERT(armed);
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
		while (g_dx11_context->GetData(disjoint.Get(), &disjoint_data, sizeof(disjoint_data), 0) != S_OK) {}
		UINT64 ticks = 0;
		while (g_dx11_context->GetData(timestamp.Get(), &ticks, sizeof(ticks), 0) != S_OK) {}
		if (disjoint_data.Disjoint || disjoint_data.Frequency == 0)
			return 0;
		return (uint64_t)((double)ticks * 1e9 / (double)disjoint_data.Frequency);
	}

private:
	Microsoft::WRL::ComPtr<ID3D11Query> timestamp;
	Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
	bool armed = false;
};

IGraphicsTimerQuery* Dx11DeviceImpl::create_timer_query() {
	return new Dx11TimerQueryImpl();
}

#undef DX11_STUB

} // namespace

void gfx_init_dx11(SDL_Window* window) {
	ASSERT(g_gfx_instance == nullptr);
	ASSERT(window != nullptr);
	if (!r_indirect_loop.get_bool()) {
		printf("Dx11: r_indirect_loop must be 1 (DX11 has no MultiDrawIndirect; see multi_draw_elements_indirect)");
		r_indirect_loop.set_bool(true);
	}
	// hello world

	HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
	ASSERT(hwnd != nullptr && "gfx_init_dx11: window has no Win32 HWND property");

	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(window, &w, &h);

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = (UINT)w;
	sd.BufferDesc.Height = (UINT)h;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT create_flags = 0;
#ifdef _DEBUG
	create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	const D3D_FEATURE_LEVEL requested_fl = D3D_FEATURE_LEVEL_11_1;
	D3D_FEATURE_LEVEL got_fl{};

	auto* impl = new Dx11DeviceImpl();
	impl->window = window;
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags,
		&requested_fl, 1, D3D11_SDK_VERSION,
		&sd, impl->swapchain.GetAddressOf(), impl->device.GetAddressOf(),
		&got_fl, impl->context.GetAddressOf());
	if (FAILED(hr))
		Fatalf("gfx_init_dx11: D3D11CreateDeviceAndSwapChain failed (hr=0x%08lx)\n", (unsigned long)hr);

	g_dx11_device = impl->device;
	g_dx11_context = impl->context;

	hr = impl->context.As(&impl->annotation);
	ASSERT(SUCCEEDED(hr) && "Dx11: ID3DUserDefinedAnnotation query failed");


	// Under a debugger (e.g. integration tests run via cdb), the SDK debug
	// layer's CORRUPTION/ERROR/WARNING messages can raise a first-chance SEH
	// exception that our crash handler treats as fatal. Log them instead.
	{
		Microsoft::WRL::ComPtr<ID3D11InfoQueue> info_queue;
		if (SUCCEEDED(impl->device.As(&info_queue))) {
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, FALSE);
		}
	}


	impl->create_backbuffer_rtv();
	spirv_compile_init();

	sys_print(Debug, "DX11 device created, feature level 0x%x, backbuffer %dx%d\n", (unsigned)got_fl, w, h);

	g_gfx_instance = impl;
}
