#pragma once
// RmlUi RenderInterface backed by raw modern OpenGL calls, same style/scope
// as the vendored ImGui OpenGL3 backend (bypasses IGraphicsDevice's
// abstracted draw path intentionally - this is third-party render glue).
// Implements only the required base-rendering functions; the optional
// layer/filter/shader functions are left at RenderInterface's no-op
// defaults, so RCSS `filter`/`backdrop-filter`/custom decorators compile
// and run but do not yet visually composite (documented as a follow-up).

#include <RmlUi/Core/RenderInterface.h>
#include <unordered_map>

class OpenGlRmlUiRenderInterface : public Rml::RenderInterface {
public:
	OpenGlRmlUiRenderInterface();
	~OpenGlRmlUiRenderInterface() override;

	// Call once per frame before Context::Render(), with the current
	// viewport size in pixels, and once after to restore engine GL state.
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
		unsigned int vao = 0;
		unsigned int vbo = 0;
		unsigned int ebo = 0;
		int index_count = 0;
	};

	unsigned int shader_program = 0;
	int uniform_translation = -1;
	int uniform_projection = -1;
	int viewport_width = 0;
	int viewport_height = 0;

	std::unordered_map<Rml::CompiledGeometryHandle, CompiledGeometry> geometry_map;
	Rml::CompiledGeometryHandle next_geometry_handle = 1;

	std::unordered_map<Rml::TextureHandle, unsigned int> texture_map;
	Rml::TextureHandle next_texture_handle = 1;

	unsigned int white_texture = 0;
};
