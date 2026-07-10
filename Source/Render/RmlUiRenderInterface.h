#pragma once
// RmlUi RenderInterface implemented entirely through IGraphicsDevice's
// abstraction (IGraphicsTexture/IGraphicsBuffer/IGraphicsVertexInput,
// RenderPipelineState, push constants) - no raw graphics-API calls, so this
// works unmodified on any backend IGraphicsDevice supports.

#include <RmlUi/Core/RenderInterface.h>
#include <glm/glm.hpp>
#include <unordered_map>

class IGraphicsShader;
class IGraphicsTexture;
class IGraphicsVertexInput;
class IGraphicsBuffer;

class RmlUiRenderInterface : public Rml::RenderInterface {
public:
	RmlUiRenderInterface();
	~RmlUiRenderInterface() override;

	// Call once per frame before Context::Render(), with the current
	// viewport size in pixels, and once after to restore engine gfx state.
	// Draws into whatever render pass is currently bound - does not open one
	// itself; the caller (Renderer::scene_draw_internal) already set up the
	// target via gfx().set_render_pass() before calling this.
	void begin_frame(int viewport_w, int viewport_h);
	void end_frame();

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

private:
	struct CompiledGeometry {
		IGraphicsBuffer* vbo = nullptr;
		IGraphicsBuffer* ebo = nullptr;
		IGraphicsVertexInput* vao = nullptr;
		int index_count = 0;
	};

	// Texture entries created via GenerateTexture own their IGraphicsTexture
	// and must release() it; entries loaded via LoadTexture point at a
	// Texture asset's gpu_ptr owned by the asset system, so must not.
	struct LoadedTexture {
		IGraphicsTexture* tex = nullptr;
		bool owns_texture = false;
	};

	IGraphicsShader* shader_program = nullptr;
	int viewport_width = 0;
	int viewport_height = 0;
	glm::mat4 projection{ 1.f };

	std::unordered_map<Rml::CompiledGeometryHandle, CompiledGeometry> geometry_map;
	Rml::CompiledGeometryHandle next_geometry_handle = 1;

	std::unordered_map<Rml::TextureHandle, LoadedTexture> texture_map;
	Rml::TextureHandle next_texture_handle = 1;

	// 1x1 white texture used for untextured geometry (RmlUi passes handle 0
	// for "no texture"; RenderGeometry substitutes this instead).
	IGraphicsTexture* white_texture = nullptr;
};
