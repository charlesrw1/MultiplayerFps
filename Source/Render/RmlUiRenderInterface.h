#pragma once
// RmlUi RenderInterface implemented entirely through IGraphicsDevice's
// abstraction (IGraphicsTexture/IGraphicsBuffer/IGraphicsVertexInput,
// RenderPipelineState, push constants) - no raw graphics-API calls, so this
// works unmodified on any backend IGraphicsDevice supports.

#include <RmlUi/Core/RenderInterface.h>
#include <glm/glm.hpp>
#include <cstddef>
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
	// viewport size in pixels and the render target the caller already bound
	// via gfx().set_render_pass() (needed to restore the base layer when the
	// layer stack pops back to it - see PopLayer()), and once after to
	// restore engine gfx state.
	void begin_frame(int viewport_w, int viewport_h, IGraphicsTexture* base_target);
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

	// Layer stack + filters (`filter: blur()/drop-shadow()`, `opacity<1`
	// stacking contexts). See docs/ui/rmlui_agent_guide.md and GH #22.
	Rml::LayerHandle PushLayer() override;
	void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode,
		Rml::Span<const Rml::CompiledFilterHandle> filters) override;
	void PopLayer() override;

	Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
	void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

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

	// ---- Layer stack + filters (PushLayer/PopLayer/CompositeLayers/CompileFilter) ----
	// See GH #22 for the design writeup this section implements.

	enum class FilterType { Opacity, Blur, DropShadow };
	// CompiledFilterHandle is an opaque uintptr_t per RmlUi's contract (Types.h);
	// the backend owns whatever it points to. RmlUi calls CompileFilter/ReleaseFilter
	// in pairs like any other compile/release handle pair in this interface, so a
	// plain new/delete here mirrors RmlUi's own reference GL3 backend exactly and is
	// the one place in this file that's fine to depart from the engine's normal
	// unique_ptr convention (there's no natural container to own it in instead - the
	// handle IS the ownership token, same shape as CompiledGeometryHandle/TextureHandle).
	struct CompiledFilter {
		FilterType type = FilterType::Opacity;
		float value = 1.f;      // Opacity
		float sigma = 0.f;      // Blur, DropShadow
		glm::vec2 offset{ 0.f }; // DropShadow, pixels
		glm::vec4 color{ 0.f, 0.f, 0.f, 1.f }; // DropShadow, premultiplied
	};

	struct RenderLayer { IGraphicsTexture* color = nullptr; };
	// [0] is always the base layer - its ->color aliases base_target (set fresh
	// every begin_frame(), not owned/released here). Layers above it are owned
	// viewport-sized offscreen textures, created via create_viewport_texture().
	std::vector<RenderLayer> layer_stack;
	// PopLayer() returns its texture here instead of releasing it, so a later
	// PushLayer() (same frame or a later one) can reuse it - same pooling
	// rationale as geometry_pool above. Drained on viewport resize.
	std::vector<IGraphicsTexture*> layer_pool;
	IGraphicsTexture* base_target = nullptr;

	// Postprocess ping-pong buffers for running filters - viewport-sized RGBA8,
	// created lazily on first use (most documents never push a layer). Resized
	// (released + recreated) alongside layer_pool on viewport size change.
	IGraphicsTexture* pp_primary = nullptr;
	IGraphicsTexture* pp_secondary = nullptr;
	IGraphicsTexture* pp_temp = nullptr;
	// Fourth scratch buffer used only by render_drop_shadow to preserve the
	// element content across render_blur (which consumes primary/secondary/temp
	// as its own scratch and would otherwise clobber the copy we composite back
	// on top of the blurred shadow).
	IGraphicsTexture* pp_temp2 = nullptr;

	IGraphicsShader* passthrough_shader = nullptr; // layer composite + opacity filter
	IGraphicsShader* blur_shader = nullptr;
	IGraphicsShader* drop_shadow_shader = nullptr;
	// Empty vertex input for gl_VertexID-only fullscreen-triangle draws (matches
	// Renderer::get_empty_vao()'s pattern, DrawLocal_Init.cpp - this class has no
	// access to that instance so it keeps its own tiny copy).
	IGraphicsBuffer* empty_vb = nullptr;
	IGraphicsVertexInput* empty_vao = nullptr;

	IGraphicsTexture* create_viewport_texture();
	void ensure_postprocess_buffers();
	// Draws a fullscreen triangle with `program` sampling `src` into `dst`
	// (full viewport extent, no blending). frag_consts/size may be null/0.
	void draw_fullscreen(IGraphicsShader* program, IGraphicsTexture* dst, IGraphicsTexture* src,
		const void* frag_consts, size_t frag_consts_size);
	// Blurs `buf` in place (reads and overwrites it), using the other two of
	// {pp_primary, pp_secondary, pp_temp} as scratch space.
	void render_blur(float sigma, IGraphicsTexture* buf);
	// Renders the drop-shadow filter starting from `cur`'s contents, returning
	// (via `cur`) whichever pp_* buffer now holds the result.
	void render_drop_shadow(const CompiledFilter& filter, IGraphicsTexture*& cur);
};
