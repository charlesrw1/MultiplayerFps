// Dx11Buffer and Dx11VertexInput — GPU buffer management + vertex input
// element description (the ID3D11InputLayout itself is built lazily by D3's
// set_pipeline against the bound VS bytecode).
#include "Dx11Local.h"
#include "Framework/Util.h"

#include <cstring>

namespace {

DXGI_FORMAT dx11_vertex_attrib_format(GraphicsVertexAttribType type, int count) {
	using gvat = GraphicsVertexAttribType;
	ASSERT(count >= 1 && count <= 4);
	switch (type) {
	case gvat::u8:
		switch (count) { case 1: return DXGI_FORMAT_R8_UINT; case 2: return DXGI_FORMAT_R8G8_UINT; case 4: return DXGI_FORMAT_R8G8B8A8_UINT; }
		break;
	case gvat::u8_normalized:
		switch (count) { case 1: return DXGI_FORMAT_R8_UNORM; case 2: return DXGI_FORMAT_R8G8_UNORM; case 4: return DXGI_FORMAT_R8G8B8A8_UNORM; }
		break;
	case gvat::i8:
		switch (count) { case 1: return DXGI_FORMAT_R8_SINT; case 2: return DXGI_FORMAT_R8G8_SINT; case 4: return DXGI_FORMAT_R8G8B8A8_SINT; }
		break;
	case gvat::i8_normalized:
		switch (count) { case 1: return DXGI_FORMAT_R8_SNORM; case 2: return DXGI_FORMAT_R8G8_SNORM; case 4: return DXGI_FORMAT_R8G8B8A8_SNORM; }
		break;
	case gvat::u16:
		switch (count) { case 1: return DXGI_FORMAT_R16_UINT; case 2: return DXGI_FORMAT_R16G16_UINT; case 4: return DXGI_FORMAT_R16G16B16A16_UINT; }
		break;
	case gvat::u16_normalized:
		switch (count) { case 1: return DXGI_FORMAT_R16_UNORM; case 2: return DXGI_FORMAT_R16G16_UNORM; case 4: return DXGI_FORMAT_R16G16B16A16_UNORM; }
		break;
	case gvat::i16:
		switch (count) { case 1: return DXGI_FORMAT_R16_SINT; case 2: return DXGI_FORMAT_R16G16_SINT; case 4: return DXGI_FORMAT_R16G16B16A16_SINT; }
		break;
	case gvat::i16_normalized:
		switch (count) { case 1: return DXGI_FORMAT_R16_SNORM; case 2: return DXGI_FORMAT_R16G16_SNORM; case 4: return DXGI_FORMAT_R16G16B16A16_SNORM; }
		break;
	case gvat::float32:
		switch (count) { case 1: return DXGI_FORMAT_R32_FLOAT; case 2: return DXGI_FORMAT_R32G32_FLOAT; case 3: return DXGI_FORMAT_R32G32B32_FLOAT; case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT; }
		break;
	}
	ASSERT(0 && "Dx11: unsupported vertex attribute type/count combination");
	return DXGI_FORMAT_R32_FLOAT;
}

UINT dx11_buffer_bind_flags(GraphicsBufferUseFlags flags) {
	UINT bind = 0;
	if (flags & BUFFER_USE_AS_VB) bind |= D3D11_BIND_VERTEX_BUFFER;
	if (flags & BUFFER_USE_AS_IB) bind |= D3D11_BIND_INDEX_BUFFER;
	if (flags & BUFFER_USE_AS_STORAGE_READ) bind |= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	if (flags & BUFFER_USE_AS_INDIRECT) bind |= D3D11_BIND_UNORDERED_ACCESS;
	if (bind == 0) bind = D3D11_BIND_CONSTANT_BUFFER;
	return bind;
}

UINT dx11_buffer_misc_flags(GraphicsBufferUseFlags flags) {
	UINT misc = 0;
	if (flags & (BUFFER_USE_AS_STORAGE_READ | BUFFER_USE_AS_INDIRECT))
		misc |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	if (flags & BUFFER_USE_AS_INDIRECT)
		misc |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	return misc;
}

} // namespace

void Dx11Buffer::recreate(int size, const void* initial_data) {
	ASSERT(size >= 0);
	buf.Reset();
	srv_cache.Reset();
	uav_cache.Reset();
	buffer_size = size;
	if (size == 0)
		return;

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = (UINT)size;
	desc.Usage = usage;
	desc.BindFlags = bind_flags;
	desc.MiscFlags = misc_flags;
	if (usage == D3D11_USAGE_DYNAMIC)
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA init{};
	init.pSysMem = initial_data;
	HRESULT hr = g_dx11_device->CreateBuffer(&desc, initial_data ? &init : nullptr, buf.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateBuffer failed");
}

void Dx11Buffer::upload(const void* data, int size) {
	ASSERT(size >= 0);
	if (size != buffer_size) {
		recreate(size, data);
		return;
	}
	if (size == 0)
		return;
	if (usage == D3D11_USAGE_DYNAMIC) {
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HRESULT hr = g_dx11_context->Map(buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		ASSERT(SUCCEEDED(hr) && "Dx11: Map(WRITE_DISCARD) failed");
		memcpy(mapped.pData, data, size);
		g_dx11_context->Unmap(buf.Get(), 0);
	} else {
		g_dx11_context->UpdateSubresource(buf.Get(), 0, nullptr, data, 0, 0);
	}
}

void Dx11Buffer::sub_upload(const void* data, int size, int offset) {
	ASSERT(size >= 0 && offset >= 0 && offset + size <= buffer_size);
	if (size == 0)
		return;
	if (usage == D3D11_USAGE_DYNAMIC) {
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HRESULT hr = g_dx11_context->Map(buf.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
		ASSERT(SUCCEEDED(hr) && "Dx11: Map(WRITE_NO_OVERWRITE) failed");
		memcpy((uint8_t*)mapped.pData + offset, data, size);
		g_dx11_context->Unmap(buf.Get(), 0);
	} else {
		D3D11_BOX box{};
		box.left = (UINT)offset;
		box.right = (UINT)(offset + size);
		box.top = 0; box.bottom = 1;
		box.front = 0; box.back = 1;
		g_dx11_context->UpdateSubresource(buf.Get(), 0, &box, data, size, 0);
	}
}

ID3D11ShaderResourceView* Dx11Buffer::get_srv() {
	ASSERT((bind_flags & D3D11_BIND_SHADER_RESOURCE) && "Dx11: buffer was not created with storage-read flag");
	if (srv_cache)
		return srv_cache.Get();
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	desc.BufferEx.NumElements = (UINT)(buffer_size / 4);
	desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
	HRESULT hr = g_dx11_device->CreateShaderResourceView(buf.Get(), &desc, srv_cache.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateShaderResourceView (buffer) failed");
	return srv_cache.Get();
}

ID3D11UnorderedAccessView* Dx11Buffer::get_uav() {
	ASSERT((bind_flags & D3D11_BIND_UNORDERED_ACCESS) && "Dx11: buffer was not created with a UAV-capable flag");
	if (uav_cache)
		return uav_cache.Get();
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = (UINT)(buffer_size / 4);
	desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	HRESULT hr = g_dx11_device->CreateUnorderedAccessView(buf.Get(), &desc, uav_cache.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateUnorderedAccessView (buffer) failed");
	return uav_cache.Get();
}

ID3D11ShaderResourceView* Dx11Buffer::get_srv_range(int offset, int size) {
	ASSERT((bind_flags & D3D11_BIND_SHADER_RESOURCE) && "Dx11: buffer was not created with storage-read flag");
	ASSERT(offset >= 0 && size > 0 && (offset % 4) == 0 && (size % 4) == 0);
	auto key = std::make_pair(offset, size);
	auto it = srv_range_cache.find(key);
	if (it != srv_range_cache.end())
		return it->second.Get();
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	desc.BufferEx.FirstElement = (UINT)(offset / 4);
	desc.BufferEx.NumElements = (UINT)(size / 4);
	desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	HRESULT hr = g_dx11_device->CreateShaderResourceView(buf.Get(), &desc, srv.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateShaderResourceView (buffer range) failed");
	auto* ptr = srv.Get();
	srv_range_cache[key] = std::move(srv);
	return ptr;
}

ID3D11UnorderedAccessView* Dx11Buffer::get_uav_range(int offset, int size) {
	ASSERT((bind_flags & D3D11_BIND_UNORDERED_ACCESS) && "Dx11: buffer was not created with a UAV-capable flag");
	ASSERT(offset >= 0 && size > 0 && (offset % 4) == 0 && (size % 4) == 0);
	auto key = std::make_pair(offset, size);
	auto it = uav_range_cache.find(key);
	if (it != uav_range_cache.end())
		return it->second.Get();
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = (UINT)(offset / 4);
	desc.Buffer.NumElements = (UINT)(size / 4);
	desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
	HRESULT hr = g_dx11_device->CreateUnorderedAccessView(buf.Get(), &desc, uav.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateUnorderedAccessView (buffer range) failed");
	auto* ptr = uav.Get();
	uav_range_cache[key] = std::move(uav);
	return ptr;
}

IGraphicsBuffer* dx11_create_buffer(const CreateBufferArgs& args) {
	ASSERT(args.size >= 0);
	Dx11Buffer* b = new Dx11Buffer();
	b->usage = (args.flags & BUFFER_USE_DYNAMIC) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	b->bind_flags = dx11_buffer_bind_flags(args.flags);
	b->misc_flags = dx11_buffer_misc_flags(args.flags);
	b->recreate(args.size, nullptr);
	return b;
}

IGraphicsVertexInput* dx11_create_vertex_input(const CreateVertexInputArgs& args) {
	ASSERT(args.vertex != nullptr);
	Dx11VertexInput* vi = new Dx11VertexInput();
	vi->index_type = args.index_type;
	vi->vertex_buffer = ((Dx11Buffer*)args.vertex)->buf;
	vi->index_buffer = args.index ? ((Dx11Buffer*)args.index)->buf : nullptr;

	for (const auto& attr : args.layout) {
		if (vi->vertex_stride == 0)
			vi->vertex_stride = (UINT)attr.stride;
		ASSERT((UINT)attr.stride == vi->vertex_stride && "Dx11: all vertex attributes must share one interleaved stride");

		D3D11_INPUT_ELEMENT_DESC elem{};
		elem.SemanticName = "TEXCOORD";
		elem.SemanticIndex = (UINT)attr.index;
		elem.Format = dx11_vertex_attrib_format(attr.type, attr.count);
		elem.InputSlot = 0;
		elem.AlignedByteOffset = (UINT)attr.offset;
		elem.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elem.InstanceDataStepRate = 0;
		vi->elements.push_back(elem);
	}
	return vi;
}
