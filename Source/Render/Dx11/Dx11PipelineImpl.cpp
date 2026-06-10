// Pipeline state-object cache for D3 (set_pipeline). Maps RenderPipelineState
// fields to cached ID3D11RasterizerState/DepthStencilState/BlendState objects,
// and caches ID3D11InputLayout per (vertex-input, shader) pair.
#include "Dx11Local.h"
#include "Framework/Util.h"

#include <cstring>

D3D11_PRIMITIVE_TOPOLOGY dx11_primitive_topology(GraphicsPrimitiveType mode) {
	switch (mode) {
	case GraphicsPrimitiveType::Triangles:     return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case GraphicsPrimitiveType::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case GraphicsPrimitiveType::Lines:         return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	}
	ASSERT(0 && "dx11_primitive_topology: unknown mode");
	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

DXGI_FORMAT dx11_index_format(VertexInputIndexType type) {
	return type == VertexInputIndexType::uint16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
}

namespace {

template <typename T>
std::vector<uint8_t> bytes_of(const T& v) {
	const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
	return std::vector<uint8_t>(p, p + sizeof(T));
}

} // namespace

ID3D11RasterizerState* Dx11StateCache::get_rasterizer_state(bool backface_culling, bool cull_front_face,
															  GraphicsFillMode fill_mode, bool poly_offset_enabled,
															  float poly_offset_factor, float poly_offset_units) {
	struct Key {
		bool backface_culling, cull_front_face, poly_offset_enabled;
		GraphicsFillMode fill_mode;
		float poly_offset_factor, poly_offset_units;
	} key{backface_culling, cull_front_face, poly_offset_enabled, fill_mode, poly_offset_factor, poly_offset_units};

	auto bytes = bytes_of(key);
	auto it = rasterizer_cache.find(bytes);
	if (it != rasterizer_cache.end())
		return it->second.Get();

	D3D11_RASTERIZER_DESC desc{};
	desc.FillMode = (fill_mode == GraphicsFillMode::Line) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	desc.CullMode = !backface_culling ? D3D11_CULL_NONE : (cull_front_face ? D3D11_CULL_FRONT : D3D11_CULL_BACK);
	// GL default front face is CCW; match that here (D4 origin/winding audit
	// may revisit if the SPIR-V->DXBC pipeline flips winding).
	desc.FrontCounterClockwise = TRUE;
	desc.DepthBias = (LONG)poly_offset_units;
	desc.SlopeScaledDepthBias = poly_offset_enabled ? poly_offset_factor : 0.f;
	desc.DepthBiasClamp = 0.f;
	desc.DepthClipEnable = TRUE;
	desc.ScissorEnable = TRUE;
	if (!poly_offset_enabled)
		desc.DepthBias = 0;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
	HRESULT hr = g_dx11_device->CreateRasterizerState(&desc, state.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateRasterizerState failed");
	auto* ptr = state.Get();
	rasterizer_cache[bytes] = std::move(state);
	return ptr;
}

ID3D11DepthStencilState* Dx11StateCache::get_depth_stencil_state(bool depth_testing, bool depth_writes, bool depth_less_than) {
	struct Key { bool depth_testing, depth_writes, depth_less_than; } key{depth_testing, depth_writes, depth_less_than};
	auto bytes = bytes_of(key);
	auto it = depth_stencil_cache.find(bytes);
	if (it != depth_stencil_cache.end())
		return it->second.Get();

	D3D11_DEPTH_STENCIL_DESC desc{};
	desc.DepthEnable = depth_testing;
	desc.DepthWriteMask = depth_writes ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	// Reverse-Z convention (matches GL_LEQUAL/GL_GEQUAL under glClipControl ZERO_TO_ONE + reversed depth clear).
	desc.DepthFunc = depth_less_than ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_GREATER_EQUAL;
	desc.StencilEnable = FALSE;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
	HRESULT hr = g_dx11_device->CreateDepthStencilState(&desc, state.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateDepthStencilState failed");
	auto* ptr = state.Get();
	depth_stencil_cache[bytes] = std::move(state);
	return ptr;
}

ID3D11BlendState* Dx11StateCache::get_blend_state(BlendState blend, const RenderPipelineState::ColorWriteMask masks[RenderPipelineState::MAX_COLOR_ATTACHMENTS]) {
	struct Key { BlendState blend; RenderPipelineState::ColorWriteMask masks[RenderPipelineState::MAX_COLOR_ATTACHMENTS]; } key{};
	key.blend = blend;
	for (int i = 0; i < RenderPipelineState::MAX_COLOR_ATTACHMENTS; i++)
		key.masks[i] = masks[i];
	auto bytes = bytes_of(key);
	auto it = blend_cache.find(bytes);
	if (it != blend_cache.end())
		return it->second.Get();

	D3D11_BLEND_DESC desc{};
	desc.IndependentBlendEnable = TRUE;
	for (int i = 0; i < RenderPipelineState::MAX_COLOR_ATTACHMENTS; i++) {
		auto& rt = desc.RenderTarget[i];
		rt.BlendEnable = (blend != BlendState::OPAQUE);
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		switch (blend) {
		case BlendState::OPAQUE:
			rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_ZERO; break;
		case BlendState::ADD:
			rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_ONE; break;
		case BlendState::BLEND:
			rt.SrcBlend = D3D11_BLEND_SRC_ALPHA; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; break;
		case BlendState::MULT:
			rt.SrcBlend = D3D11_BLEND_DEST_COLOR; rt.DestBlend = D3D11_BLEND_ZERO; break;
		case BlendState::SCREEN:
			rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_INV_SRC_COLOR; break;
		case BlendState::PREMULT_BLEND:
			rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; break;
		}
		const auto& m = masks[i];
		rt.RenderTargetWriteMask =
			(m.r ? D3D11_COLOR_WRITE_ENABLE_RED   : 0) |
			(m.g ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0) |
			(m.b ? D3D11_COLOR_WRITE_ENABLE_BLUE  : 0) |
			(m.a ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0);
	}

	Microsoft::WRL::ComPtr<ID3D11BlendState> state;
	HRESULT hr = g_dx11_device->CreateBlendState(&desc, state.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateBlendState failed");
	auto* ptr = state.Get();
	blend_cache[bytes] = std::move(state);
	return ptr;
}

ID3D11InputLayout* Dx11StateCache::get_input_layout(Dx11VertexInput* vao, Dx11ShaderImpl* shader) {
	ASSERT(vao != nullptr && shader != nullptr);
	// get_empty_vao() has no attributes (e.g. fullscreen-triangle passes that
	// generate vertices in the VS from SV_VertexID). CreateInputLayout rejects
	// a NULL pInputElementDescs even for NumElements==0, so skip it entirely;
	// IASetInputLayout(nullptr) is valid when the VS declares no inputs.
	if (vao->elements.empty())
		return nullptr;

	auto key = std::make_pair(vao, shader);
	auto it = input_layout_cache.find(key);
	if (it != input_layout_cache.end())
		return it->second.Get();

	Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
	HRESULT hr = g_dx11_device->CreateInputLayout(vao->elements.data(), (UINT)vao->elements.size(),
												   shader->vs_bytecode.data(), shader->vs_bytecode.size(),
												   layout.GetAddressOf());
	if (FAILED(hr)) {
		Microsoft::WRL::ComPtr<ID3D11InfoQueue> iq;
		if (SUCCEEDED(g_dx11_device.As(&iq))) {
			UINT64 n = iq->GetNumStoredMessages();
			for (UINT64 i = 0; i < n; i++) {
				SIZE_T len = 0;
				iq->GetMessage(i, nullptr, &len);
				std::vector<uint8_t> mbuf(len);
				D3D11_MESSAGE* msg = (D3D11_MESSAGE*)mbuf.data();
				iq->GetMessage(i, msg, &len);
				sys_print(Error, "Dx11 InfoQueue: %.*s\n", (int)msg->DescriptionByteLength, msg->pDescription);
			}
			iq->ClearStoredMessages();
		}
	}
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateInputLayout failed");
	auto* ptr = layout.Get();
	input_layout_cache[key] = std::move(layout);
	return ptr;
}
