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

#undef near
#undef far

// Backbuffer wrapper returned by Dx11DeviceImpl::acquire_swapchain_texture().
// Like the GL swapchain sentinel, set_render_pass / blit_textures recognize
// this by pointer identity rather than treating it as a normal texture.
// D2 fleshes out the general Dx11Texture (real SRV/UAV/DSV creation); for D1
// it only carries the backbuffer RTV + dims.
class Dx11Texture : public IGraphicsTexture
{
public:
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	int width = 0;
	int height = 0;

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return 0; }

	bool is_compressed() const override { return false; }
	int get_num_mips() const override { return 1; }
	glm::ivec2 get_size() const override { return { width, height }; }
	GraphicsTextureFormat get_texture_format() const override { return GraphicsTextureFormat::rgba8; }
	GraphicsTextureType get_texture_type() const override { return GraphicsTextureType::t2D; }
	int get_compressed_stride() const override { return 0; }

	void sub_image_upload(int, int, int, int, int, int, const void*) override { ASSERT(false && "Dx11: D2"); }
	void sub_image_upload_3d(int, int, int, int, int, int, int, const void*) override { ASSERT(false && "Dx11: D2"); }
	void clear_image() override { ASSERT(false && "Dx11: D2"); }
	void set_mip_range(int, int) override { ASSERT(false && "Dx11: D2"); }
	void generate_mipmaps() override { ASSERT(false && "Dx11: D2"); }
	void download(int, int, void*, int) override { ASSERT(false && "Dx11: D2"); }
};
