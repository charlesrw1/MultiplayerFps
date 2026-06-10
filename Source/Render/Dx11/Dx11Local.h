#pragma once
// Shared types for the DX11 backend (Source/Render/Dx11/*). Mirrors the role
// of OpenGlDeviceLocal.h for the GL backend.
//
// d3d11.h pulls in <windows.h>, whose old memory-model macros (min/max/
// near/far) collide with std::/glm:: usage and parameter names elsewhere in
// the engine. All Dx11/*.cpp TUs are excluded from the unity build (see
// CsRemake.vcxproj) so this pollution stays local to this directory.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Render/IGraphicsDevice.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <map>

#undef near
#undef far

// Device/context globals, set by gfx_init_dx11 and used by every Dx11*Impl.cpp
// factory (none of them can see Dx11DeviceImpl, which is anonymous-namespace).
extern Microsoft::WRL::ComPtr<ID3D11Device> g_dx11_device;
extern Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_dx11_context;

// ---------------------------------------------------------------------------
// Texture format mapping (Dx11TextureImpl.cpp)
// ---------------------------------------------------------------------------

// DXGI format triple for a GraphicsTextureFormat: the format the resource is
// allocated with, and the formats used for SRV and RTV/DSV/UAV views (differ
// for typeless depth formats).
struct Dx11FormatInfo
{
	DXGI_FORMAT resource_format = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT rtv_dsv_format = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT uav_format = DXGI_FORMAT_UNKNOWN;
	bool is_depth = false;
	bool is_compressed = false;
	// rgb8/rgb16f have no DX11 3-channel texture format; the resource is
	// allocated as the 4-channel equivalent and sub_image_upload repacks.
	bool widen_rgb_to_rgba = false;
};
Dx11FormatInfo dx11_texture_format_info(GraphicsTextureFormat fmt);

// ---------------------------------------------------------------------------
// Dx11Texture — general texture (also used as the swapchain-backbuffer
// wrapper; see is_backbuffer).
// ---------------------------------------------------------------------------
class Dx11Texture : public IGraphicsTexture
{
public:
	// Regular texture constructor (Dx11TextureImpl.cpp).
	explicit Dx11Texture(const CreateTextureArgs& args);
	// Backbuffer wrapper constructor: only rtv/width/height are valid.
	Dx11Texture() = default;

	~Dx11Texture() override;

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return 0; }

	bool is_compressed() const override { return fmt_info.is_compressed; }
	int get_num_mips() const override { return mips; }
	glm::ivec2 get_size() const override { return { width, height }; }
	GraphicsTextureFormat get_texture_format() const override { return my_fmt; }
	GraphicsTextureType get_texture_type() const override { return my_type; }
	int get_compressed_stride() const override;

	void sub_image_upload(int layer, int x, int y, int w, int h, int size, const void* data) override;
	void sub_image_upload_3d(int z, int layer, int x, int y, int w, int h, int size, const void* data) override;
	void clear_image() override;
	void set_mip_range(int base, int max) override;
	void generate_mipmaps() override;
	void download(int mip, int layer, void* dest, int dest_size_bytes) override;
	int get_mem_usage() const override { return mem_usage; }

	// ---- DX11-internal accessors (D3 binds / render passes) ---------------
	// Lazily create + cache views. layer == -1 selects all-array/all-face view.
	ID3D11ShaderResourceView* get_srv();
	ID3D11RenderTargetView* get_rtv(int mip, int layer);
	ID3D11DepthStencilView* get_dsv(int mip, int layer);
	ID3D11UnorderedAccessView* get_uav(int mip, int layer);

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv; // backbuffer-only
	int width = 0;
	int height = 0;
	int depth_or_layers = 1;
	int mips = 1;
	int mem_usage = 0;
	bool is_backbuffer = false;
	GraphicsTextureType my_type{};
	GraphicsTextureFormat my_fmt{};
	Dx11FormatInfo fmt_info{};

private:
	int srv_base_mip = 0;
	int srv_max_mip = 0; // set to mips - 1 in the args constructor
	std::map<int64_t, Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> rtv_cache;
	std::map<int64_t, Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> dsv_cache;
	std::map<int64_t, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uav_cache;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_cache;
};

// ---------------------------------------------------------------------------
// Factory functions, mirroring OpenGlDeviceLocal.h's opengl_create_* family.
// ---------------------------------------------------------------------------
IGraphicsBuffer* dx11_create_buffer(const CreateBufferArgs& args);
IGraphicsVertexInput* dx11_create_vertex_input(const CreateVertexInputArgs& args);
IGraphicsTexture* dx11_create_texture(const CreateTextureArgs& args);
IGraphicsSampler* dx11_create_sampler(const CreateSamplerArgs& args);

IGraphicsShader* dx11_create_shader_vert_frag(const std::string& vert_path,
											  const std::string& frag_path,
											  const std::string& defines);
IGraphicsShader* dx11_create_shader_compute(const std::string& compute_path,
											const std::string& defines);
IGraphicsShader* dx11_create_shader_single_file(const std::string& shared_path,
												 const std::string& defines);

// D3D11_INPUT_ELEMENT_DESC + raw vertex/index buffers + topology metadata for
// a created vertex input. Built by Dx11VertexInputImpl; consumed by D3's
// set_pipeline (which builds/caches the ID3D11InputLayout against the bound
// VS bytecode).
class Dx11VertexInput : public IGraphicsVertexInput
{
public:
	void release() override { delete this; }
	uint32_t get_internal_handle() override { return 0; }

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
	// Stride of the single interleaved vertex buffer (all attribs share it,
	// matching VertexLayout::stride from CreateVertexInputArgs).
	UINT vertex_stride = 0;
	VertexInputIndexType index_type{};
};

class Dx11Buffer : public IGraphicsBuffer
{
public:
	void release() override { delete this; }
	uint32_t get_internal_handle() override { return 0; }
	void upload(const void* data, int size) override;
	void sub_upload(const void* data, int size, int offset) override;
	int get_buf_size() const override { return buffer_size; }

	Microsoft::WRL::ComPtr<ID3D11Buffer> buf;
	int buffer_size = 0;
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	UINT bind_flags = 0;
	UINT misc_flags = 0;

	// Recreate `buf` with a new size, preserving usage/bind/misc flags.
	void recreate(int size, const void* initial_data);

	// Lazily-created ByteAddressBuffer views over the whole buffer, used for
	// storage-buffer (SRV/UAV) and indirect-args (UAV, for clear_buffer_uint32)
	// binds. Only valid when the corresponding bind flag was set at creation.
	ID3D11ShaderResourceView* get_srv();
	ID3D11UnorderedAccessView* get_uav();

private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_cache;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav_cache;
};
