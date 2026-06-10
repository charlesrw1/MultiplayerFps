// DX11 backend skeleton (P3.1 D1). Boots a D3D11 device + swapchain and can
// clear the backbuffer; everything else is an ASSERT(false) stub filled in by
// later sub-phases (D2 resources, D3 pipeline/draws, D4 origin audit, D5
// imgui/tail). See docs/rendering/gfx_abstraction_nextsteps.md.

#include "Dx11Local.h"
#include "Framework/Util.h"

#include <SDL3/SDL.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

#define DX11_STUB ASSERT(false && "Dx11: not yet implemented")

class Dx11DeviceImpl : public IGraphicsDevice
{
public:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain;
	Dx11Texture* backbuffer = nullptr;

	~Dx11DeviceImpl() override {
		if (backbuffer) {
			backbuffer->release();
			backbuffer = nullptr;
		}
	}

	GraphicsDeviceType get_device_type() override { return GraphicsDeviceType::Dx11; }

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
		backbuffer->width = (int)desc.Width;
		backbuffer->height = (int)desc.Height;
	}

	// ---- Frame lifecycle ---------------------------------------------------
	void begin_frame() override {}
	IGraphicsTexture* acquire_swapchain_texture() override {
		ASSERT(backbuffer != nullptr);
		return backbuffer;
	}
	void submit_and_present() override {
		swapchain->Present(0, 0);
	}

	// ---- Render pass / clear (D1 scope) ------------------------------------
	void set_render_pass(const RenderPassState& state) override {
		ASSERT(state.depth_info == nullptr && "Dx11: depth targets are D2");
		ASSERT(state.color_infos.size() <= 1 && "Dx11: MRT is D2/D3");

		ID3D11RenderTargetView* rtv = nullptr;
		if (!state.color_infos.empty()) {
			const ColorTargetInfo& info = state.color_infos[0];
			ASSERT(info.texture == backbuffer && "Dx11: only the swapchain target is wired up in D1");
			rtv = backbuffer->rtv.Get();
			if (info.wants_clear) {
				const float c[4] = { info.clear_color.r, info.clear_color.g, info.clear_color.b, info.clear_color.a };
				context->ClearRenderTargetView(rtv, c);
			}
		}
		context->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : nullptr, nullptr);
	}

	void set_viewport(int x, int y, int w, int h) override {
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = (float)x;
		vp.TopLeftY = (float)y;
		vp.Width = (float)w;
		vp.Height = (float)h;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		context->RSSetViewports(1, &vp);
	}

	void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value) override { DX11_STUB; }
	void blit_textures(const GraphicsBlitInfo& info) override { DX11_STUB; }

	// ---- Pipeline state (D3) ------------------------------------------------
	void set_pipeline(const RenderPipelineState& state) override { DX11_STUB; }
	void set_depth_write_enabled(bool enabled) override { DX11_STUB; }
	IGraphicsShader* get_active_shader() override { DX11_STUB; return nullptr; }
	void reset_state_cache() override { DX11_STUB; }

	// ---- Resources (D2) ------------------------------------------------------
	IGraphicsTexture* create_texture(const CreateTextureArgs& args) override { DX11_STUB; return nullptr; }
	IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) override { DX11_STUB; return nullptr; }
	IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) override { DX11_STUB; return nullptr; }
	IGraphicsSampler* create_sampler(const CreateSamplerArgs& args) override { DX11_STUB; return nullptr; }

	// ---- Scissor / draws (D3) ------------------------------------------------
	void set_scissor(int x, int y, int w, int h) override { DX11_STUB; }
	void disable_scissor() override { DX11_STUB; }
	void draw_elements_base_vertex(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset, int base_vertex) override { DX11_STUB; }
	void draw_arrays(GraphicsPrimitiveType mode, int first, int count) override { DX11_STUB; }
	void draw_elements(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset) override { DX11_STUB; }
	void draw_elements_indirect(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int byte_offset) override { DX11_STUB; }
	void draw_elements_instanced_base_vertex_base_instance(GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type, int byte_offset, int instance_count, int base_vertex, uint32_t base_instance) override { DX11_STUB; }
	void multi_draw_elements_indirect(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int byte_offset, int draw_count, int stride, const void* client_ptr) override { DX11_STUB; }
	void multi_draw_elements_indirect_count(GraphicsPrimitiveType mode, VertexInputIndexType index_type, IGraphicsBuffer* indirect, int indirect_byte_offset, IGraphicsBuffer* count, int count_byte_offset, int max_draw_count, int stride) override { DX11_STUB; }

	void wait_for_gpu_idle() override { DX11_STUB; }

	// ---- Binds (D3) -----------------------------------------------------------
	void bind_texture(int slot, IGraphicsTexture* tex) override { DX11_STUB; }
	void bind_uniform_buffer_base(int slot, IGraphicsBuffer* buf) override { DX11_STUB; }
	void bind_sampler(int slot, IGraphicsSampler* sampler) override { DX11_STUB; }
	void bind_storage_buffer_base(int slot, IGraphicsBuffer* buf) override { DX11_STUB; }
	void bind_storage_buffer_range(int slot, IGraphicsBuffer* buf, int offset, int size) override { DX11_STUB; }
	void bind_image_for_compute(int slot, IGraphicsTexture* tex, int mip, int layer, GraphicsImageAccess access) override { DX11_STUB; }

	// ---- Push constants (D3) ---------------------------------------------------
	void push_vertex_constants(int slot, const void* data, int size) override { DX11_STUB; }
	void push_fragment_constants(int slot, const void* data, int size) override { DX11_STUB; }
	void push_compute_constants(int slot, const void* data, int size) override { DX11_STUB; }

	// ---- Compute (D3) -----------------------------------------------------------
	void begin_compute_pass() override { DX11_STUB; }
	void dispatch_compute(int groups_x, int groups_y, int groups_z) override { DX11_STUB; }
	void memory_barrier(uint32_t bits) override { DX11_STUB; }

	// ---- Buffer clear/readback (D2) ----------------------------------------------
	void clear_buffer_uint32(IGraphicsBuffer* buf, uint32_t value) override { DX11_STUB; }
	void download_buffer(IGraphicsBuffer* buf, int offset, int size, void* dest) override { DX11_STUB; }

	// ---- Misc state (D3/D4) -------------------------------------------------------
	void set_line_width(float width) override { DX11_STUB; }
	void set_polygon_fill_mode(GraphicsFillMode mode) override { DX11_STUB; }
	void copy_texture(IGraphicsTexture* src, int src_mip, int src_layer, IGraphicsTexture* dst, int dst_mip, int dst_layer, int w, int h) override { DX11_STUB; }

	// ---- Debug groups + timer queries (D5) -----------------------------------------
	void push_debug_group(const char* name) override {}
	void pop_debug_group() override {}
	IGraphicsTimerQuery* create_timer_query() override { DX11_STUB; return nullptr; }

	// ---- Window / vsync / imgui (D5) -------------------------------------------------
	void set_vsync(bool enable) override { DX11_STUB; }
	void imgui_init() override { DX11_STUB; }
	void imgui_shutdown() override { DX11_STUB; }
	void imgui_new_frame() override { DX11_STUB; }
	void imgui_render_draw_data() override { DX11_STUB; }
	bool imgui_process_event(const union SDL_Event* event) override { DX11_STUB; return false; }

	// ---- Shader factory (D2) -----------------------------------------------------------
	IGraphicsShader* create_shader_vert_frag(const std::string& vert_path, const std::string& frag_path, const std::string& defines) override { DX11_STUB; return nullptr; }
	IGraphicsShader* create_shader_vert_frag_geo(const std::string& vert_path, const std::string& frag_path, const std::string& geo_path, const std::string& defines) override { DX11_STUB; return nullptr; }
	IGraphicsShader* create_shader_compute(const std::string& compute_path, const std::string& defines) override { DX11_STUB; return nullptr; }
	IGraphicsShader* create_shader_single_file(const std::string& shared_path, const std::string& defines) override { DX11_STUB; return nullptr; }
	IGraphicsShader* create_shader_single_file_tess(const std::string& shared_path, const std::string& defines) override { DX11_STUB; return nullptr; }
};

#undef DX11_STUB

} // namespace

void gfx_init_dx11(SDL_Window* window) {
	ASSERT(g_gfx_instance == nullptr);
	ASSERT(window != nullptr);

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
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags,
		&requested_fl, 1, D3D11_SDK_VERSION,
		&sd, impl->swapchain.GetAddressOf(), impl->device.GetAddressOf(),
		&got_fl, impl->context.GetAddressOf());
	if (FAILED(hr))
		Fatalf("gfx_init_dx11: D3D11CreateDeviceAndSwapChain failed (hr=0x%08lx)\n", (unsigned long)hr);

	impl->create_backbuffer_rtv();

	sys_print(Debug, "DX11 device created, feature level 0x%x, backbuffer %dx%d\n", (unsigned)got_fl, w, h);

	g_gfx_instance = impl;
}
