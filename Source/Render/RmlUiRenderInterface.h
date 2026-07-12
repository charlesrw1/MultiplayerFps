#pragma once
// RmlUi RenderInterface implemented entirely through IGraphicsDevice's
// abstraction (IGraphicsTexture/IGraphicsBuffer/IGraphicsVertexInput,
// RenderPipelineState, push constants) - no raw graphics-API calls, so this
// works unmodified on any backend IGraphicsDevice supports.

#include <RmlUi/Core/RenderInterface.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

class IGraphicsShader;
class IGraphicsTexture;
class IGraphicsVertexInput;
class IGraphicsBuffer;
class Texture;

// Per-frame counters for the RmlUi debug menu (RmlUiDebugMenu.cpp) - reset in
// begin_frame(), incremented from the RenderInterface calls below, read by
// ImGui after end_frame() so a full frame's worth is visible at once.
// Compile/release counts are the interesting ones for diagnosing animation
// cost: RmlUi has no separate "just update alpha" fast path through this
// interface, so an animated opacity/property forces full ReleaseGeometry +
// CompileGeometry (fresh IGraphicsBuffer/IGraphicsVertexInput allocation and
// a full vertex re-upload) every frame it changes, not just a cheap
// per-frame value tweak.
struct RmlUiRenderStats {
	int compile_geometry_calls = 0;
	int release_geometry_calls = 0;
	int render_geometry_calls = 0;
	int load_texture_calls = 0;
	int generate_texture_calls = 0;
	size_t vertex_bytes_uploaded = 0;
	size_t index_bytes_uploaded = 0;
	// Of this frame's compile_geometry_calls, how many needed a brand new
	// GPU buffer/VAO vs. reused a pooled one - should settle near 0 once the
	// pool has warmed up to a document's steady-state element count.
	int gpu_objects_created = 0;
};
extern RmlUiRenderStats g_rmlui_render_stats;

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

	// RCSS `transform`/`transition`/`@keyframes` on transform properties.
	// nullptr resets to identity (RmlUi calls this whenever the transform
	// stack changes, including popping back to "no transform").
	void SetTransform(const Rml::Matrix4f* transform) override;

private:
	struct CompiledGeometry {
		IGraphicsBuffer* vbo = nullptr;
		IGraphicsBuffer* ebo = nullptr;
		IGraphicsVertexInput* vao = nullptr;
		int index_count = 0;
	};

	// Texture entries created via GenerateTexture own a private IGraphicsTexture
	// and must release() it. Entries loaded via LoadTexture instead keep the
	// Texture* asset pointer and re-read ->gpu_ptr live every draw, rather than
	// caching the IGraphicsTexture* once - Texture::load() returns a normal
	// g_assets-cached asset that the file watcher can hot-reload at any time
	// (unlike the old Texture::force_load_for_ui "system asset", which opted
	// out of that), and a hot-reload frees + replaces gpu_ptr. Caching the raw
	// pointer here left it dangling the moment that happened (confirmed via
	// crash: editing ui/heart_icon.tis mid-run freed the texture RenderGeometry
	// was about to bind).
	struct LoadedTexture {
		IGraphicsTexture* owned_tex = nullptr; // GenerateTexture only
		Texture* asset = nullptr;              // LoadTexture only
		bool owns_texture = false;
	};

	IGraphicsShader* shader_program = nullptr;
	int viewport_width = 0;
	int viewport_height = 0;
	glm::mat4 projection{ 1.f };
	glm::mat4 transform{ 1.f };

	std::unordered_map<Rml::CompiledGeometryHandle, CompiledGeometry> geometry_map;
	Rml::CompiledGeometryHandle next_geometry_handle = 1;

	// ReleaseGeometry() stashes the {vbo,ebo,vao} triple here instead of
	// destroying it; CompileGeometry() pulls from here before allocating new
	// GPU objects. RmlUi calls Release+Compile every frame for any element
	// whose opacity is animating (opacity is baked into vertex colours, so
	// there's no cheaper "just update alpha" path) - reusing objects via
	// upload() (respecifies an existing buffer's contents) instead of
	// create/destroy per frame is what actually matters, since profiling
	// showed the cost was GPU object lifecycle call count, not upload bytes.
	// The VAO's attribute layout is identical for every RmlUi geometry, so
	// any pooled triple is valid for any new geometry - no size/shape
	// matching needed. Uncapped: RmlUi's own element count already bounds
	// how large this can get (it's however many geometry chunks the biggest
	// document shown this session had live at once), and the destructor
	// releases everything on shutdown either way.
	std::vector<CompiledGeometry> geometry_pool;

	std::unordered_map<Rml::TextureHandle, LoadedTexture> texture_map;
	Rml::TextureHandle next_texture_handle = 1;

	// 1x1 white texture used for untextured geometry (RmlUi passes handle 0
	// for "no texture"; RenderGeometry substitutes this instead).
	IGraphicsTexture* white_texture = nullptr;
};
