// Dx11Texture — texture creation, format mapping, views, upload/download.
// Mirrors OpenGlTextureImpl.cpp. Also defines Dx11SamplerImpl.
#include "Dx11Local.h"
#include "Framework/Util.h"

#include <vector>
#include <cstring>
#include <algorithm>

namespace {

int64_t view_key(int mip, int layer) { return ((int64_t)mip << 32) | (uint32_t)(layer + 1); }

// Texture2D-shaped types (array slices = depth_or_layers; cubemaps store
// 6*num_cubes slices).
bool is_2d_array_shaped(GraphicsTextureType t) {
	return t == GraphicsTextureType::t2DArray || t == GraphicsTextureType::tCubemap ||
		   t == GraphicsTextureType::tCubemapArray;
}

} // namespace

Dx11FormatInfo dx11_texture_format_info(GraphicsTextureFormat fmt) {
	Dx11FormatInfo f;
	using gtf = GraphicsTextureFormat;
	switch (fmt) {
	case gtf::r8:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R8_UNORM;
		break;
	case gtf::rg8:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R8G8_UNORM;
		break;
	case gtf::rgb8:
		// No 24-bit DX11 format; widen to RGBA8 (sub_image_upload repacks).
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		f.widen_rgb_to_rgba = true;
		break;
	case gtf::rgba8:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case gtf::r16f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R16_FLOAT;
		break;
	case gtf::rg16f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R16G16_FLOAT;
		break;
	case gtf::rgb16f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		f.widen_rgb_to_rgba = true;
		break;
	case gtf::rgba16f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case gtf::r32f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R32_FLOAT;
		break;
	case gtf::rg32f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case gtf::bc1:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC1_UNORM;
		f.is_compressed = true;
		break;
	case gtf::bc1_srgb:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC1_UNORM_SRGB;
		f.is_compressed = true;
		break;
	case gtf::bc3:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC3_UNORM;
		f.is_compressed = true;
		break;
	case gtf::bc4:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC4_UNORM;
		f.is_compressed = true;
		break;
	case gtf::bc5:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC5_UNORM;
		f.is_compressed = true;
		break;
	case gtf::bc6:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_BC6H_UF16;
		f.is_compressed = true;
		break;
	case gtf::depth16f:
		f.resource_format = DXGI_FORMAT_R16_TYPELESS;
		f.srv_format = DXGI_FORMAT_R16_UNORM;
		f.rtv_dsv_format = DXGI_FORMAT_D16_UNORM;
		f.is_depth = true;
		break;
	case gtf::depth24f:
	case gtf::depth24stencil8:
		f.resource_format = DXGI_FORMAT_R24G8_TYPELESS;
		f.srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		f.rtv_dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		f.is_depth = true;
		break;
	case gtf::depth32f:
		f.resource_format = DXGI_FORMAT_R32_TYPELESS;
		f.srv_format = DXGI_FORMAT_R32_FLOAT;
		f.rtv_dsv_format = DXGI_FORMAT_D32_FLOAT;
		f.is_depth = true;
		break;
	case gtf::r11f_g11f_b10f:
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_R11G11B10_FLOAT;
		break;
	case gtf::rgba16_snorm:
		f.resource_format = f.srv_format = f.rtv_dsv_format = f.uav_format = DXGI_FORMAT_R16G16B16A16_SNORM;
		break;
	default:
		ASSERT(0 && "dx11_texture_format_info: unknown format");
		f.resource_format = f.srv_format = f.rtv_dsv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	}
	return f;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Dx11Texture::Dx11Texture(const CreateTextureArgs& args) {
	ASSERT(args.width > 0 && args.height > 0 && args.num_mip_maps >= 1);
	width = args.width;
	height = args.height;
	mips = args.num_mip_maps;
	my_type = args.type;
	my_fmt = args.format;
	fmt_info = dx11_texture_format_info(my_fmt);
	srv_max_mip = mips - 1;

	depth_or_layers = (my_type == GraphicsTextureType::t3D) ? std::max(1, args.depth_3d)
				   : (my_type == GraphicsTextureType::tCubemap) ? 6
				   : (my_type == GraphicsTextureType::tCubemapArray) ? 6 * std::max(1, args.depth_3d)
				   : (my_type == GraphicsTextureType::t2DArray) ? std::max(1, args.depth_3d)
				   : 1;

	UINT bind_flags = D3D11_BIND_SHADER_RESOURCE;
	if (!fmt_info.is_depth && !fmt_info.is_compressed)
		bind_flags |= D3D11_BIND_RENDER_TARGET;
	if (fmt_info.is_depth)
		bind_flags |= D3D11_BIND_DEPTH_STENCIL;
	if (fmt_info.uav_format != DXGI_FORMAT_UNKNOWN)
		bind_flags |= D3D11_BIND_UNORDERED_ACCESS;

	UINT misc_flags = 0;
	// D3D11_RESOURCE_MISC_GENERATE_MIPS is for textures whose mip chain is
	// generated at runtime via GenerateMips() (single mip supplied at
	// creation). It also restricts UpdateSubresource to whole-subresource,
	// mip-0-only updates - incompatible with assets (e.g. DDS) that upload a
	// full pre-baked mip chain.
	if (mips == 1 && (bind_flags & D3D11_BIND_RENDER_TARGET) && (bind_flags & D3D11_BIND_SHADER_RESOURCE)) {
		misc_flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		generates_mips = true;
	}
	if (my_type == GraphicsTextureType::tCubemap || my_type == GraphicsTextureType::tCubemapArray)
		misc_flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr;
	if (my_type == GraphicsTextureType::t3D) {
		D3D11_TEXTURE3D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Depth = depth_or_layers;
		desc.MipLevels = mips;
		desc.Format = fmt_info.resource_format;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = bind_flags;
		desc.MiscFlags = misc_flags;
		Microsoft::WRL::ComPtr<ID3D11Texture3D> tex3d;
		hr = g_dx11_device->CreateTexture3D(&desc, nullptr, tex3d.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: CreateTexture3D failed");
		resource = tex3d;
	} else {
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = mips;
		desc.ArraySize = depth_or_layers;
		desc.Format = fmt_info.resource_format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = bind_flags;
		desc.MiscFlags = misc_flags;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
		hr = g_dx11_device->CreateTexture2D(&desc, nullptr, tex2d.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: CreateTexture2D failed");
		resource = tex2d;
	}
}

Dx11Texture::~Dx11Texture() {}

int Dx11Texture::get_compressed_stride() const {
	ASSERT(fmt_info.is_compressed);
	if (my_fmt == GraphicsTextureFormat::bc1 || my_fmt == GraphicsTextureFormat::bc1_srgb ||
		my_fmt == GraphicsTextureFormat::bc4)
		return 8;
	return 16;
}

// ---------------------------------------------------------------------------
// Upload / download
// ---------------------------------------------------------------------------

// Repacks tightly-packed 3-channel source data into 4-channel (alpha = max),
// for the rgb8 -> RGBA8_UNORM and rgb16f -> RGBA16_FLOAT widen cases.
namespace {
void widen_rgb_to_rgba(const void* data, int w, int h, bool is_16f, std::vector<uint8_t>& storage) {
	const int src_channels = 3;
	const int dst_channels = 4;
	const int comp_size = is_16f ? 2 : 1;
	storage.resize((size_t)w * h * dst_channels * comp_size);
	const uint8_t* src = (const uint8_t*)data;
	uint8_t* dst = storage.data();
	for (int i = 0; i < w * h; i++) {
		memcpy(dst + i * dst_channels * comp_size, src + i * src_channels * comp_size, src_channels * comp_size);
		if (is_16f) {
			uint16_t one = 0x3C00; // half-float 1.0
			memcpy(dst + (i * dst_channels + 3) * comp_size, &one, comp_size);
		} else {
			dst[i * dst_channels + 3] = 0xFF;
		}
	}
}

// TEMP D7 diagnostic: D3D11_3SDKLayers raises a SEH exception (ReportCorruption)
// for invalid-parameter UpdateSubresource calls regardless of
// ID3D11InfoQueue::SetBreakOnSeverity, so the only way to read the actual
// validation message under a debugger is to catch it here, drain the info
// queue, and re-throw.
int dx11_dump_info_queue_and_continue(ID3D11Device* dev) {
	Microsoft::WRL::ComPtr<ID3D11InfoQueue> iq;
	if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(iq.GetAddressOf())))) {
		UINT64 n = iq->GetNumStoredMessages();
		for (UINT64 i = 0; i < n; i++) {
			SIZE_T len = 0;
			iq->GetMessage(i, nullptr, &len);
			std::vector<uint8_t> buf(len);
			D3D11_MESSAGE* msg = (D3D11_MESSAGE*)buf.data();
			iq->GetMessage(i, msg, &len);
			sys_print(Error, "Dx11 InfoQueue: %.*s\n", (int)msg->DescriptionByteLength, msg->pDescription);
		}
		iq->ClearStoredMessages();
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

void update_subresource_diag(ID3D11DeviceContext* ctx, ID3D11Device* dev, ID3D11Resource* resource, int level,
							  const D3D11_BOX* box, const void* data, UINT row_pitch) {
	__try {
		ctx->UpdateSubresource(resource, level, box, data, row_pitch, 0);
	} __except (dx11_dump_info_queue_and_continue(dev)) {
	}
}
} // namespace

void Dx11Texture::sub_image_upload(int level, int x, int y, int w, int h, int size, const void* data) {
	ASSERT(w > 0 && h > 0 && resource);
	// GL's glTexSubImage2D(..., nullptr) with no PBO bound is a no-op (used by
	// callers like BRDFIntegration::run to allocate an uninitialized texture).
	// UpdateSubresource requires non-null pSrcData, so mirror that as a no-op.
	if (!data)
		return;
	D3D11_BOX box{};
	box.left = x; box.right = x + w;
	box.top = y; box.bottom = y + h;
	box.front = 0; box.back = 1;

	const bool is_16f = (my_fmt == GraphicsTextureFormat::rgb16f);
	std::vector<uint8_t> widened;
	const void* upload_data = data;
	UINT row_pitch = 0;
	if (fmt_info.is_compressed) {
		row_pitch = (UINT)((w + 3) / 4) * get_compressed_stride();
	} else if (fmt_info.widen_rgb_to_rgba) {
		widen_rgb_to_rgba(data, w, h, is_16f, widened);
		upload_data = widened.data();
		row_pitch = (UINT)w * 4 * (is_16f ? 2 : 1);
	} else {
		row_pitch = (UINT)(size / h);
	}
	// Block-compressed formats require box coords aligned to the 4x4 block
	// size; small mips (1x1/2x2) violate that. Such uploads always cover the
	// whole mip, so pass a null box (whole-subresource update) instead.
	// D3D11 debug-layer requires pDstBox == nullptr (whole-subresource update)
	// for resources created with D3D11_RESOURCE_MISC_GENERATE_MIPS, and for
	// block-compressed formats a box smaller than 4x4 (small mips). Both
	// cases here always cover the whole mip (x==y==0).
	const bool whole_subresource = generates_mips ||
		(fmt_info.is_compressed && (x != 0 || y != 0 || (w % 4) != 0 || (h % 4) != 0));
	Microsoft::WRL::ComPtr<ID3D11Device> dev;
	resource->GetDevice(dev.GetAddressOf());
	update_subresource_diag(g_dx11_context.Get(), dev.Get(), resource.Get(), level,
							 whole_subresource ? nullptr : &box, upload_data, row_pitch);
}

void Dx11Texture::sub_image_upload_3d(int z, int level, int x, int y, int w, int h, int size, const void* data) {
	ASSERT(w > 0 && h > 0 && resource);
	D3D11_BOX box{};
	box.left = x; box.right = x + w;
	box.top = y; box.bottom = y + h;
	box.front = z; box.back = z + 1;

	UINT row_pitch = fmt_info.is_compressed ? (UINT)((w + 3) / 4) * get_compressed_stride() : (UINT)(size / h);
	const bool is_array = is_2d_array_shaped(my_type);
	const UINT subresource = is_array ? D3D11CalcSubresource(level, 0, mips) : level;
	g_dx11_context->UpdateSubresource(resource.Get(), subresource, &box, data, row_pitch, 0);
}

void Dx11Texture::clear_image() {
	ASSERT(!fmt_info.is_compressed && !fmt_info.is_depth);
	const float zero[4] = {0.f, 0.f, 0.f, 0.f};
	for (int layer = 0; layer < depth_or_layers; layer++)
		for (int mip = 0; mip < mips; mip++)
			g_dx11_context->ClearRenderTargetView(get_rtv(mip, depth_or_layers > 1 ? layer : -1), zero);
}

void Dx11Texture::set_mip_range(int base, int max) {
	ASSERT(base >= 0 && max >= base);
	// SRV mip range is fixed at view-creation time on DX11; drop the cached
	// SRV so the next get_srv() rebuilds it with [base, max].
	srv_base_mip = base;
	srv_max_mip = max;
	srv_cache.Reset();
}

void Dx11Texture::generate_mipmaps() {
	ASSERT(mips > 1);
	g_dx11_context->GenerateMips(get_srv());
}

void Dx11Texture::download(int mip, int layer, void* dest, int dest_size_bytes) {
	ASSERT(dest != nullptr && dest_size_bytes > 0 && resource);
	ASSERT(my_type == GraphicsTextureType::t2D || layer >= 0);

	D3D11_TEXTURE2D_DESC src_desc{};
	((ID3D11Texture2D*)resource.Get())->GetDesc(&src_desc);

	D3D11_TEXTURE2D_DESC staging_desc = src_desc;
	staging_desc.Width = std::max(1, width >> mip);
	staging_desc.Height = std::max(1, height >> mip);
	staging_desc.MipLevels = 1;
	staging_desc.ArraySize = 1;
	staging_desc.Usage = D3D11_USAGE_STAGING;
	staging_desc.BindFlags = 0;
	staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	staging_desc.MiscFlags = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = g_dx11_device->CreateTexture2D(&staging_desc, nullptr, staging.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: download staging texture create failed");

	const UINT src_subresource = is_2d_array_shaped(my_type) ? D3D11CalcSubresource(mip, std::max(layer, 0), mips) : mip;
	g_dx11_context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, resource.Get(), src_subresource, nullptr);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = g_dx11_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	ASSERT(SUCCEEDED(hr) && "Dx11: download Map failed");

	const int rows = staging_desc.Height;
	const int row_bytes = dest_size_bytes / rows;
	uint8_t* dst = (uint8_t*)dest;
	for (int r = 0; r < rows; r++)
		memcpy(dst + r * row_bytes, (const uint8_t*)mapped.pData + r * mapped.RowPitch, row_bytes);

	g_dx11_context->Unmap(staging.Get(), 0);
}

// ---------------------------------------------------------------------------
// View accessors
// ---------------------------------------------------------------------------
ID3D11ShaderResourceView* Dx11Texture::get_srv() {
	if (srv_cache)
		return srv_cache.Get();

	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = fmt_info.srv_format;
	const UINT mip_levels = (UINT)(srv_max_mip - srv_base_mip + 1);
	switch (my_type) {
	case GraphicsTextureType::t2D:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = srv_base_mip;
		desc.Texture2D.MipLevels = mip_levels;
		break;
	case GraphicsTextureType::t2DArray:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MostDetailedMip = srv_base_mip;
		desc.Texture2DArray.MipLevels = mip_levels;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = depth_or_layers;
		break;
	case GraphicsTextureType::t3D:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MostDetailedMip = srv_base_mip;
		desc.Texture3D.MipLevels = mip_levels;
		break;
	case GraphicsTextureType::tCubemap:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MostDetailedMip = srv_base_mip;
		desc.TextureCube.MipLevels = mip_levels;
		break;
	case GraphicsTextureType::tCubemapArray:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		desc.TextureCubeArray.MostDetailedMip = srv_base_mip;
		desc.TextureCubeArray.MipLevels = mip_levels;
		desc.TextureCubeArray.First2DArrayFace = 0;
		desc.TextureCubeArray.NumCubes = depth_or_layers / 6;
		break;
	}
	HRESULT hr = g_dx11_device->CreateShaderResourceView(resource.Get(), &desc, srv_cache.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateShaderResourceView failed");
	return srv_cache.Get();
}

ID3D11RenderTargetView* Dx11Texture::get_rtv(int mip, int layer) {
	const int64_t key = view_key(mip, layer);
	auto it = rtv_cache.find(key);
	if (it != rtv_cache.end())
		return it->second.Get();

	D3D11_RENDER_TARGET_VIEW_DESC desc{};
	desc.Format = fmt_info.rtv_dsv_format;
	if (my_type == GraphicsTextureType::t3D) {
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mip;
		desc.Texture3D.FirstWSlice = std::max(layer, 0);
		desc.Texture3D.WSize = (layer < 0) ? depth_or_layers : 1;
	} else if (is_2d_array_shaped(my_type) || layer >= 0) {
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mip;
		desc.Texture2DArray.FirstArraySlice = std::max(layer, 0);
		desc.Texture2DArray.ArraySize = (layer < 0) ? depth_or_layers : 1;
	} else {
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mip;
	}
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
	HRESULT hr = g_dx11_device->CreateRenderTargetView(resource.Get(), &desc, view.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateRenderTargetView failed");
	rtv_cache[key] = view;
	return view.Get();
}

ID3D11DepthStencilView* Dx11Texture::get_dsv(int mip, int layer) {
	const int64_t key = view_key(mip, layer);
	auto it = dsv_cache.find(key);
	if (it != dsv_cache.end())
		return it->second.Get();

	D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
	desc.Format = fmt_info.rtv_dsv_format;
	if (is_2d_array_shaped(my_type) || layer >= 0) {
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mip;
		desc.Texture2DArray.FirstArraySlice = std::max(layer, 0);
		desc.Texture2DArray.ArraySize = (layer < 0) ? depth_or_layers : 1;
	} else {
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mip;
	}
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view;
	HRESULT hr = g_dx11_device->CreateDepthStencilView(resource.Get(), &desc, view.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateDepthStencilView failed");
	dsv_cache[key] = view;
	return view.Get();
}

ID3D11UnorderedAccessView* Dx11Texture::get_uav(int mip, int layer) {
	ASSERT(fmt_info.uav_format != DXGI_FORMAT_UNKNOWN && "Dx11: texture format has no UAV");
	const int64_t key = view_key(mip, layer);
	auto it = uav_cache.find(key);
	if (it != uav_cache.end())
		return it->second.Get();

	D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = fmt_info.uav_format;
	if (my_type == GraphicsTextureType::t3D) {
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mip;
		desc.Texture3D.FirstWSlice = std::max(layer, 0);
		desc.Texture3D.WSize = (layer < 0) ? depth_or_layers : 1;
	} else if (is_2d_array_shaped(my_type) || layer >= 0) {
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mip;
		desc.Texture2DArray.FirstArraySlice = std::max(layer, 0);
		desc.Texture2DArray.ArraySize = (layer < 0) ? depth_or_layers : 1;
	} else {
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mip;
	}
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> view;
	HRESULT hr = g_dx11_device->CreateUnorderedAccessView(resource.Get(), &desc, view.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateUnorderedAccessView failed");
	uav_cache[key] = view;
	return view.Get();
}

// ---------------------------------------------------------------------------
// Dx11SamplerImpl
// ---------------------------------------------------------------------------
namespace {

D3D11_FILTER dx11_sampler_filter(const CreateSamplerArgs& args) {
	const bool min_linear = args.min_filter != GraphicsSamplerFilter::Nearest;
	const bool mag_linear = args.mag_filter != GraphicsSamplerFilter::Nearest;
	const bool mip_linear = args.min_filter == GraphicsSamplerFilter::LinearMipmapLinear;

	D3D11_FILTER base;
	if (!min_linear && !mag_linear && !mip_linear)
		base = D3D11_FILTER_MIN_MAG_MIP_POINT;
	else if (min_linear && mag_linear && mip_linear)
		base = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	else {
		UINT bits = 0;
		if (min_linear) bits |= 0x10; // D3D11_FILTER_TYPE_LINEAR << MIN shift
		if (mag_linear) bits |= 0x4;
		if (mip_linear) bits |= 0x1;
		base = (D3D11_FILTER)bits;
	}

	switch (args.reduction) {
	case GraphicsSamplerReduction::Min: return (D3D11_FILTER)(base | D3D11_FILTER_REDUCTION_TYPE_MINIMUM << 7);
	case GraphicsSamplerReduction::Max: return (D3D11_FILTER)(base | D3D11_FILTER_REDUCTION_TYPE_MAXIMUM << 7);
	default: return base;
	}
}

D3D11_TEXTURE_ADDRESS_MODE dx11_sampler_wrap(GraphicsSamplerWrap wrap) {
	switch (wrap) {
	case GraphicsSamplerWrap::Repeat:      return D3D11_TEXTURE_ADDRESS_WRAP;
	case GraphicsSamplerWrap::ClampToEdge: return D3D11_TEXTURE_ADDRESS_CLAMP;
	}
	return D3D11_TEXTURE_ADDRESS_CLAMP;
}

class Dx11SamplerImpl : public IGraphicsSampler
{
public:
	explicit Dx11SamplerImpl(const CreateSamplerArgs& args) {
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = dx11_sampler_filter(args);
		desc.AddressU = desc.AddressV = desc.AddressW = dx11_sampler_wrap(args.wrap);
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		HRESULT hr = g_dx11_device->CreateSamplerState(&desc, sampler.GetAddressOf());
		ASSERT(SUCCEEDED(hr) && "Dx11: CreateSamplerState failed");
	}

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return 0; }

	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
};

} // namespace

IGraphicsTexture* dx11_create_texture(const CreateTextureArgs& args) {
	ASSERT(args.width > 0 && args.height > 0 && args.num_mip_maps >= 1);
	return new Dx11Texture(args);
}

IGraphicsSampler* dx11_create_sampler(const CreateSamplerArgs& args) {
	return new Dx11SamplerImpl(args);
}

ID3D11SamplerState* dx11_sampler_state(IGraphicsSampler* sampler) {
	ASSERT(sampler != nullptr);
	return ((Dx11SamplerImpl*)sampler)->sampler.Get();
}
