#include "RmlUiRenderInterface.h"
#include "IGraphicsDevice.h"
#include "Texture.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "../Shaders/ShaderBufferShared.txt"
#include "Framework/Log.h"
#include "Framework/Util.h"
#include <RmlUi/Core/Dictionary.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

RmlUiRenderStats g_rmlui_render_stats;

RmlUiRenderInterface::RmlUiRenderInterface() {
	shader_program = gfx().create_shader_vert_frag("RmlUiV.txt", "RmlUiF.txt");
	passthrough_shader = gfx().create_shader_vert_frag("fullscreenquad.txt", "RmlUiFilterPassthroughF.txt");
	blur_shader = gfx().create_shader_vert_frag("fullscreenquad.txt", "RmlUiBlurF.txt");
	drop_shadow_shader = gfx().create_shader_vert_frag("fullscreenquad.txt", "RmlUiDropShadowF.txt");

	CreateTextureArgs white_args;
	white_args.type = GraphicsTextureType::t2D;
	white_args.format = GraphicsTextureFormat::rgba8;
	white_args.width = 1;
	white_args.height = 1;
	white_args.num_mip_maps = 1;
	white_args.sampler_type = GraphicsSamplerType::NearestClamped;
	white_texture = gfx().create_texture(white_args);
	unsigned char white_pixel[4] = { 255, 255, 255, 255 };
	white_texture->sub_image_upload(0, 0, 0, 1, 1, 0, white_pixel);

	// Empty VAO for gl_VertexID-only fullscreen-triangle draws (fullscreenquad.txt
	// reads gl_VertexID, never the VB contents - see DrawLocal_Init.cpp:320-330 for
	// the identical pattern this mirrors).
	CreateBufferArgs vb_args;
	vb_args.size = 12 * 3;
	vb_args.flags = (GraphicsBufferUseFlags)BUFFER_USE_AS_VB;
	empty_vb = gfx().create_buffer(vb_args);
	CreateVertexInputArgs vargs;
	vargs.vertex = empty_vb;
	empty_vao = gfx().create_vertex_input(vargs);
}

RmlUiRenderInterface::~RmlUiRenderInterface() {
	safe_release(white_texture);
	safe_release(shader_program);
	safe_release(passthrough_shader);
	safe_release(blur_shader);
	safe_release(drop_shadow_shader);
	safe_release(empty_vao);
	safe_release(empty_vb);
	safe_release(pp_primary);
	safe_release(pp_secondary);
	safe_release(pp_temp);
	safe_release(pp_temp2);
	for (IGraphicsTexture*& tex : layer_pool)
		safe_release(tex);
	for (auto& [handle, tex] : texture_map)
		if (tex.owns_texture)
			safe_release(tex.owned_tex);
	for (auto& [handle, geo] : geometry_map) {
		safe_release(geo.vao);
		safe_release(geo.vbo);
		safe_release(geo.ebo);
	}
	for (auto& geo : geometry_pool) {
		safe_release(geo.vao);
		safe_release(geo.vbo);
		safe_release(geo.ebo);
	}
}

void RmlUiRenderInterface::begin_frame(int viewport_w, int viewport_h, IGraphicsTexture* base_target_in) {
	if (viewport_w != viewport_width || viewport_h != viewport_height) {
		// Pooled/postprocess textures are sized to the old viewport - drop them
		// so create_viewport_texture() rebuilds at the new size on next use.
		for (IGraphicsTexture*& tex : layer_pool)
			safe_release(tex);
		layer_pool.clear();
		safe_release(pp_primary);
		safe_release(pp_secondary);
		safe_release(pp_temp);
		safe_release(pp_temp2);
	}
	viewport_width = viewport_w;
	viewport_height = viewport_h;
	// Orthographic projection matching pixel coordinates, origin top-left.
	projection = glm::ortho(0.f, (float)viewport_w, (float)viewport_h, 0.f);

	base_target = base_target_in;
	ASSERT(layer_stack.empty());
	layer_stack.push_back({ base_target });

	g_rmlui_render_stats = RmlUiRenderStats{};
}

void RmlUiRenderInterface::end_frame() {
	// Push/PopLayer calls within Context::Render() must balance back to just
	// the base layer.
	ASSERT(layer_stack.size() == 1);
	layer_stack.clear();
	gfx().disable_scissor();
}

Rml::CompiledGeometryHandle RmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	const int vertex_bytes = (int)(vertices.size() * sizeof(Rml::Vertex));
	const int index_bytes = (int)(indices.size() * sizeof(int));

	CompiledGeometry geo;
	if (!geometry_pool.empty()) {
		geo = geometry_pool.back();
		geometry_pool.pop_back();
	} else {
		CreateBufferArgs vb_args;
		vb_args.flags = (GraphicsBufferUseFlags)BUFFER_USE_AS_VB;
		geo.vbo = gfx().create_buffer(vb_args);

		CreateBufferArgs ib_args;
		ib_args.flags = (GraphicsBufferUseFlags)BUFFER_USE_AS_IB;
		geo.ebo = gfx().create_buffer(ib_args);

		const VertexLayout layout[] = {
			VertexLayout(0, 2, GraphicsVertexAttribType::float32,       sizeof(Rml::Vertex), offsetof(Rml::Vertex, position)),
			VertexLayout(1, 4, GraphicsVertexAttribType::u8_normalized, sizeof(Rml::Vertex), offsetof(Rml::Vertex, colour)),
			VertexLayout(2, 2, GraphicsVertexAttribType::float32,       sizeof(Rml::Vertex), offsetof(Rml::Vertex, tex_coord)),
		};
		CreateVertexInputArgs vargs;
		vargs.vertex = geo.vbo;
		vargs.index = geo.ebo;
		vargs.layout = layout;
		vargs.index_type = VertexInputIndexType::uint32;
		geo.vao = gfx().create_vertex_input(vargs);

		g_rmlui_render_stats.gpu_objects_created++;
	}

	// upload() respecifies an existing buffer's contents in place - no new
	// GPU object, whether this triple is freshly created above or pulled
	// from the pool.
	geo.vbo->upload(vertices.data(), vertex_bytes);
	geo.ebo->upload(indices.data(), index_bytes);
	geo.index_count = (int)indices.size();

	g_rmlui_render_stats.compile_geometry_calls++;
	g_rmlui_render_stats.vertex_bytes_uploaded += vertex_bytes;
	g_rmlui_render_stats.index_bytes_uploaded += index_bytes;

	Rml::CompiledGeometryHandle handle = next_geometry_handle++;
	geometry_map[handle] = geo;
	return handle;
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;

	IGraphicsTexture* tex = white_texture;
	// GenerateTexture entries (owns_texture) arrive already premultiplied by
	// RmlUi core itself; LoadTexture entries come straight off the shared
	// Texture asset system with straight alpha - see RmlUiFragPushConsts.
	bool needs_premultiply = false;
	if (texture != 0) {
		auto tex_it = texture_map.find(texture);
		if (tex_it != texture_map.end()) {
			const LoadedTexture& entry = tex_it->second;
			// asset->gpu_ptr is re-read live (not cached) since Texture::load's
			// hot-reload can free+replace it between LoadTexture and this draw -
			// see the LoadedTexture comment in RmlUiRenderInterface.h.
			IGraphicsTexture* resolved = entry.owns_texture ? entry.owned_tex
				: (entry.asset ? entry.asset->gpu_ptr : nullptr);
			if (resolved)
				tex = resolved;
			needs_premultiply = !entry.owns_texture;
		}
	}

	RenderPipelineState state;
	state.program = shader_program;
	state.vao = it->second.vao;
	state.blend = BlendState::PREMULT_BLEND;
	state.depth_testing = false;
	state.depth_writes = false;
	state.backface_culling = false;
	gfx().set_pipeline(state);

	gpu::RmlUiVertPushConsts pcv{};
	// Apply the element's local transform before the ortho projection so
	// `gl_Position = Projection * (Transform * (pos + translation))`.
	pcv.Projection = projection * transform;
	pcv.Translation = { translation.x, translation.y };
	gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

	gpu::RmlUiFragPushConsts pcf{};
	pcf.needs_premultiply = needs_premultiply ? 1.f : 0.f;
	gfx().push_fragment_constants(0, &pcf, sizeof(pcf));

	gfx().bind_texture(0, tex);
	gfx().draw_elements(GraphicsPrimitiveType::Triangles, it->second.index_count, VertexInputIndexType::uint32, 0);

	g_rmlui_render_stats.render_geometry_calls++;
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;

	geometry_pool.push_back(it->second);
	geometry_map.erase(it);

	g_rmlui_render_stats.release_geometry_calls++;
}

Rml::TextureHandle RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	Texture* t = Texture::load(source);
	if (!t || !t->gpu_ptr) {
		sys_print(Warning, "RmlUiRenderInterface::LoadTexture failed for source '%s'\n", source.c_str());
		texture_dimensions = { 0, 0 };
		return 0;
	}
	const glm::ivec2 size = t->get_size();
	texture_dimensions = { size.x, size.y };

	Rml::TextureHandle handle = next_texture_handle++;
	LoadedTexture entry;
	entry.asset = t;
	entry.owns_texture = false;
	texture_map[handle] = entry;
	g_rmlui_render_stats.load_texture_calls++;
	return handle;
}

Rml::TextureHandle RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	CreateTextureArgs args;
	args.type = GraphicsTextureType::t2D;
	args.format = GraphicsTextureFormat::rgba8;
	args.width = source_dimensions.x;
	args.height = source_dimensions.y;
	args.num_mip_maps = 1;
	args.sampler_type = GraphicsSamplerType::LinearClamped;
	IGraphicsTexture* tex = gfx().create_texture(args);
	tex->sub_image_upload(0, 0, 0, source_dimensions.x, source_dimensions.y, 0, source.data());

	Rml::TextureHandle handle = next_texture_handle++;
	LoadedTexture entry;
	entry.owned_tex = tex;
	entry.owns_texture = true;
	texture_map[handle] = entry;
	g_rmlui_render_stats.generate_texture_calls++;
	return handle;
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
	auto it = texture_map.find(texture);
	if (it == texture_map.end())
		return;
	if (it->second.owns_texture)
		safe_release(it->second.owned_tex);
	texture_map.erase(it);
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable) {
	if (!enable)
		gfx().disable_scissor();
	// When enable is true, RmlUi always follows up with SetScissorRegion()
	// before drawing, so there is nothing to do here (matches the deferred
	// setter pattern of the raw-GL backend this replaces).
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
	// Engine scissor origin is bottom-left; RmlUi's region origin is top-left.
	gfx().set_scissor(region.Left(), viewport_height - region.Top() - region.Height(), region.Width(), region.Height());
}

void RmlUiRenderInterface::SetTransform(const Rml::Matrix4f* new_transform) {
	if (new_transform)
		// Rml::Matrix4f is column-major float[16] (RMLUI_MATRIX_ROW_MAJOR is
		// not defined by this build), same layout as glm::mat4.
		transform = glm::make_mat4(new_transform->data());
	else
		transform = glm::mat4(1.f);
}

// ---- Layer stack + filters (GH #22) ---------------------------------------

IGraphicsTexture* RmlUiRenderInterface::create_viewport_texture() {
	CreateTextureArgs args;
	args.type = GraphicsTextureType::t2D;
	args.format = GraphicsTextureFormat::rgba8;
	args.width = viewport_width;
	args.height = viewport_height;
	args.num_mip_maps = 1;
	args.sampler_type = GraphicsSamplerType::LinearClamped;
	return gfx().create_texture(args);
}

void RmlUiRenderInterface::ensure_postprocess_buffers() {
	if (!pp_primary)
		pp_primary = create_viewport_texture();
	if (!pp_secondary)
		pp_secondary = create_viewport_texture();
	if (!pp_temp)
		pp_temp = create_viewport_texture();
	if (!pp_temp2)
		pp_temp2 = create_viewport_texture();
}

void RmlUiRenderInterface::draw_fullscreen(IGraphicsShader* program, IGraphicsTexture* dst, IGraphicsTexture* src,
	const void* frag_consts, size_t frag_consts_size) {
	ColorTargetInfo target(dst);
	auto color_infos = { target };
	RenderPassState pass;
	pass.color_infos = color_infos;
	gfx().set_render_pass(pass);
	gfx().set_viewport(0, 0, viewport_width, viewport_height);

	RenderPipelineState state;
	state.program = program;
	state.vao = empty_vao;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	state.backface_culling = false;
	gfx().set_pipeline(state);

	if (frag_consts && frag_consts_size > 0)
		gfx().push_fragment_constants(0, frag_consts, (int)frag_consts_size);

	gfx().bind_texture(0, src);
	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void RmlUiRenderInterface::render_blur(float sigma, IGraphicsTexture* buf) {
	// Ported from RmlUi's reference GL3 backend's SigmaToParameters - picks how
	// many half-scale downsample passes to do so the separable blur itself only
	// ever needs a small, sigma<=3 kernel regardless of the requested blur radius.
	constexpr int max_num_passes = 8;
	constexpr float max_single_pass_sigma = 3.0f;
	int pass_level = (int)glm::clamp(std::log2(std::max(sigma * (2.f / max_single_pass_sigma), 1.f)), 0.f, (float)max_num_passes);
	float pass_sigma = glm::clamp(sigma / float(1 << pass_level), 0.0f, max_single_pass_sigma);

	// The other two of {pp_primary, pp_secondary, pp_temp} besides `buf` are
	// free scratch space for this call.
	IGraphicsTexture* scratch_a = (buf == pp_primary) ? pp_secondary : pp_primary;
	IGraphicsTexture* scratch_b = (buf == pp_temp) ? pp_secondary : pp_temp;

	int cw = viewport_width, ch = viewport_height;
	IGraphicsTexture* src = buf;
	IGraphicsTexture* dst = scratch_a;

	// Downscale by iterative half-scaling with bilinear filtering (reduces
	// aliasing vs. a single large-radius jump), ping-ponging src/dst.
	for (int i = 0; i < pass_level; i++) {
		int nw = std::max(cw / 2, 1);
		int nh = std::max(ch / 2, 1);
		GraphicsBlitInfo blit;
		blit.src.texture = src;
		blit.src.w = cw;
		blit.src.h = ch;
		blit.dest.texture = dst;
		blit.dest.w = nw;
		blit.dest.h = nh;
		blit.filter = GraphicsFilterType::Linear;
		gfx().blit_textures(blit);
		std::swap(src, dst);
		cw = nw;
		ch = nh;
	}

	// Two-pass separable gaussian at the reduced resolution. Weights are
	// BLUR_NUM_WEIGHTS=4 taps by symmetry (RmlUi's BLUR_SIZE=7), ported from
	// SetBlurWeights.
	gpu::RmlUiBlurFragPushConsts weights_consts{};
	{
		float w[4];
		float normalization = 0.f;
		for (int i = 0; i < 4; i++) {
			w[i] = (std::abs(pass_sigma) < 0.1f) ? float(i == 0)
				: std::exp(-float(i * i) / (2.0f * pass_sigma * pass_sigma)) / (std::sqrt(2.f * glm::pi<float>()) * pass_sigma);
			normalization += (i == 0 ? 1.f : 2.0f) * w[i];
		}
		for (int i = 0; i < 4; i++)
			w[i] /= normalization;
		weights_consts.weights = glm::vec4(w[0], w[1], w[2], w[3]);
	}

	// The downsampled content occupies only the top-left cw*ch corner of the
	// full viewport-sized (W*H) ping-pong textures, so remap the fullscreen
	// quad's [0,1] texCoord onto that corner (uv_scale) and clamp every tap to
	// it (uv_limits, half-texel inset) - sampling the untouched region outside
	// the corner is what was previously eating the blurred content entirely.
	const glm::vec2 tex_dim{ (float)viewport_width, (float)viewport_height };
	weights_consts.uv_scale = glm::vec2(float(cw), float(ch)) / tex_dim;
	weights_consts.uv_limits = glm::vec4(glm::vec2(0.5f) / tex_dim,
		(glm::vec2(float(cw), float(ch)) - glm::vec2(0.5f)) / tex_dim);

	auto blur_pass = [&](glm::vec2 texel_offset) {
		gpu::RmlUiBlurFragPushConsts consts = weights_consts;
		consts.texel_offset = texel_offset;

		ColorTargetInfo target(dst);
		auto color_infos = { target };
		RenderPassState pass;
		pass.color_infos = color_infos;
		gfx().set_render_pass(pass);
		gfx().set_viewport(0, 0, cw, ch);

		RenderPipelineState state;
		state.program = blur_shader;
		state.vao = empty_vao;
		state.blend = BlendState::OPAQUE;
		state.depth_testing = false;
		state.depth_writes = false;
		state.backface_culling = false;
		gfx().set_pipeline(state);

		gfx().push_fragment_constants(0, &consts, sizeof(consts));
		gfx().bind_texture(0, src);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

		std::swap(src, dst);
	};
	// Texel offset is 1/full-texture-dimension: one downsampled pixel maps to
	// exactly one texel of the W*H texture in the top-left corner, so a step of
	// 1/W (1/H) advances one downsampled sample - not 1/cw, which would step
	// cw/W texels and blur far too wide.
	blur_pass(glm::vec2(0.f, 1.f / tex_dim.y)); // vertical
	blur_pass(glm::vec2(1.f / tex_dim.x, 0.f)); // horizontal

	// `src` ping-pongs between only `buf` and `scratch_a` above (scratch_b is
	// only ever a write target, never read back into src), so its identity
	// after an even total number of swaps (pass_level downsample swaps + 2
	// blur-pass swaps) lands back on `buf` itself. Blitting a texture into
	// itself is undefined behaviour (glBlitFramebuffer et al.) - bounce
	// through the unused scratch buffer first when that happens.
	if (src == buf) {
		GraphicsBlitInfo bounce;
		bounce.src.texture = src;
		bounce.src.w = cw;
		bounce.src.h = ch;
		bounce.dest.texture = scratch_b;
		bounce.dest.w = cw;
		bounce.dest.h = ch;
		bounce.filter = GraphicsFilterType::Nearest;
		gfx().blit_textures(bounce);
		src = scratch_b;
	}

	// Upscale the blurred downsampled image back into `buf` at full viewport
	// resolution (single blit - RmlUi's reference does an extra stability pass
	// here to avoid jitter at high pass levels; skipped for this first cut).
	GraphicsBlitInfo blit;
	blit.src.texture = src;
	blit.src.w = cw;
	blit.src.h = ch;
	blit.dest.texture = buf;
	blit.dest.w = viewport_width;
	blit.dest.h = viewport_height;
	blit.filter = GraphicsFilterType::Linear;
	gfx().blit_textures(blit);
}

void RmlUiRenderInterface::render_drop_shadow(const CompiledFilter& filter, IGraphicsTexture*& cur) {
	IGraphicsTexture* shadow = (cur == pp_primary) ? pp_secondary : pp_primary;

	gpu::RmlUiDropShadowFragPushConsts consts{};
	consts.color = filter.color;
	consts.uv_offset = filter.offset / glm::vec2(float(viewport_width), float(viewport_height));
	draw_fullscreen(drop_shadow_shader, shadow, cur, &consts, sizeof(consts));

	// render_blur consumes pp_primary/secondary/temp as scratch, one of which is
	// `cur` - stash the element content in pp_temp2 (which blur never touches) so
	// the composite-on-top below reads the real element, not blur leftovers.
	IGraphicsTexture* element = cur;
	if (filter.sigma >= 0.5f) {
		GraphicsBlitInfo keep;
		keep.src.texture = cur;
		keep.src.w = viewport_width;
		keep.src.h = viewport_height;
		keep.dest.texture = pp_temp2;
		keep.dest.w = viewport_width;
		keep.dest.h = viewport_height;
		keep.filter = GraphicsFilterType::Nearest;
		gfx().blit_textures(keep);
		element = pp_temp2;

		render_blur(filter.sigma, shadow);
	}

	// Composite the original (unshadowed) content back on top of the shadow.
	{
		ColorTargetInfo target(shadow);
		auto color_infos = { target };
		RenderPassState pass;
		pass.color_infos = color_infos;
		gfx().set_render_pass(pass);
		gfx().set_viewport(0, 0, viewport_width, viewport_height);

		RenderPipelineState state;
		state.program = passthrough_shader;
		state.vao = empty_vao;
		state.blend = BlendState::PREMULT_BLEND;
		state.depth_testing = false;
		state.depth_writes = false;
		state.backface_culling = false;
		gfx().set_pipeline(state);

		gpu::RmlUiFilterPassthroughFragPushConsts pcf{};
		pcf.opacity = 1.f;
		gfx().push_fragment_constants(0, &pcf, sizeof(pcf));
		gfx().bind_texture(0, element);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	}

	cur = shadow;
}

Rml::LayerHandle RmlUiRenderInterface::PushLayer() {
	IGraphicsTexture* tex;
	if (!layer_pool.empty()) {
		tex = layer_pool.back();
		layer_pool.pop_back();
	} else {
		tex = create_viewport_texture();
	}
	layer_stack.push_back({ tex });

	ColorTargetInfo target(tex);
	target.wants_clear = true; // transparent black
	target.clear_color = glm::vec4(0.f);
	auto color_infos = { target };
	RenderPassState pass;
	pass.color_infos = color_infos;
	gfx().set_render_pass(pass);
	gfx().set_viewport(0, 0, viewport_width, viewport_height);

	return (Rml::LayerHandle)(layer_stack.size() - 1);
}

void RmlUiRenderInterface::PopLayer() {
	ASSERT(layer_stack.size() > 1);
	IGraphicsTexture* popped = layer_stack.back().color;
	layer_stack.pop_back();
	layer_pool.push_back(popped);

	IGraphicsTexture* top = layer_stack.back().color;
	ColorTargetInfo target(top);
	auto color_infos = { target };
	RenderPassState pass;
	pass.color_infos = color_infos;
	gfx().set_render_pass(pass);
	gfx().set_viewport(0, 0, viewport_width, viewport_height);
}

void RmlUiRenderInterface::CompositeLayers(Rml::LayerHandle source_handle, Rml::LayerHandle destination_handle, Rml::BlendMode blend_mode,
	Rml::Span<const Rml::CompiledFilterHandle> filters) {
	ensure_postprocess_buffers();
	ASSERT(source_handle < layer_stack.size() && destination_handle < layer_stack.size());
	// RmlUi leaves the element scissor from the just-rendered layer enabled here;
	// every filter pass below is a full-viewport (or reduced-resolution corner)
	// operation, so that stale scissor would clip the blits and fullscreen draws.
	// Disable it - RmlUi re-issues SetScissorRegion before its next geometry draw.
	gfx().disable_scissor();

	// Filters can't read and write the same texture, and destination may equal
	// source - always start from a copy in pp_primary.
	{
		GraphicsBlitInfo blit;
		blit.src.texture = layer_stack[source_handle].color;
		blit.src.w = viewport_width;
		blit.src.h = viewport_height;
		blit.dest.texture = pp_primary;
		blit.dest.w = viewport_width;
		blit.dest.h = viewport_height;
		blit.filter = GraphicsFilterType::Nearest;
		gfx().blit_textures(blit);
	}

	IGraphicsTexture* cur = pp_primary;
	for (Rml::CompiledFilterHandle fh : filters) {
		const CompiledFilter* filter = reinterpret_cast<const CompiledFilter*>(fh);
		if (!filter)
			continue;
		switch (filter->type) {
		case FilterType::Opacity: {
			IGraphicsTexture* dst = (cur == pp_primary) ? pp_secondary : pp_primary;
			gpu::RmlUiFilterPassthroughFragPushConsts pcf{};
			pcf.opacity = filter->value;
			draw_fullscreen(passthrough_shader, dst, cur, &pcf, sizeof(pcf));
			cur = dst;
			break;
		}
		case FilterType::Blur:
			render_blur(filter->sigma, cur);
			break;
		case FilterType::DropShadow:
			render_drop_shadow(*filter, cur);
			break;
		}
	}

	IGraphicsTexture* dest_tex = layer_stack[destination_handle].color;
	{
		ColorTargetInfo target(dest_tex);
		auto color_infos = { target };
		RenderPassState pass;
		pass.color_infos = color_infos;
		gfx().set_render_pass(pass);
		gfx().set_viewport(0, 0, viewport_width, viewport_height);

		RenderPipelineState state;
		state.program = passthrough_shader;
		state.vao = empty_vao;
		state.blend = (blend_mode == Rml::BlendMode::Replace) ? BlendState::OPAQUE : BlendState::PREMULT_BLEND;
		state.depth_testing = false;
		state.depth_writes = false;
		state.backface_culling = false;
		gfx().set_pipeline(state);

		gpu::RmlUiFilterPassthroughFragPushConsts pcf{};
		pcf.opacity = 1.f;
		gfx().push_fragment_constants(0, &pcf, sizeof(pcf));
		gfx().bind_texture(0, cur);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	}

	if (destination_handle != layer_stack.size() - 1) {
		IGraphicsTexture* top = layer_stack.back().color;
		ColorTargetInfo target(top);
		auto color_infos = { target };
		RenderPassState pass;
		pass.color_infos = color_infos;
		gfx().set_render_pass(pass);
		gfx().set_viewport(0, 0, viewport_width, viewport_height);
	}
}

Rml::CompiledFilterHandle RmlUiRenderInterface::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
	CompiledFilter* filter = new CompiledFilter();
	if (name == "opacity") {
		filter->type = FilterType::Opacity;
		filter->value = Rml::Get(parameters, "value", 1.0f);
	} else if (name == "blur") {
		filter->type = FilterType::Blur;
		filter->sigma = Rml::Get(parameters, "sigma", 1.0f);
	} else if (name == "drop-shadow") {
		filter->type = FilterType::DropShadow;
		filter->sigma = Rml::Get(parameters, "sigma", 0.f);
		Rml::Colourb c = Rml::Get(parameters, "color", Rml::Colourb());
		float a = c.alpha / 255.f;
		filter->color = glm::vec4(c.red / 255.f * a, c.green / 255.f * a, c.blue / 255.f * a, a);
		Rml::Vector2f off = Rml::Get(parameters, "offset", Rml::Vector2f(0.f));
		filter->offset = glm::vec2(off.x, off.y);
	} else {
		sys_print(Warning, "RmlUiRenderInterface::CompileFilter: unsupported filter '%s'\n", name.c_str());
		delete filter;
		return 0;
	}
	return reinterpret_cast<Rml::CompiledFilterHandle>(filter);
}

void RmlUiRenderInterface::ReleaseFilter(Rml::CompiledFilterHandle filter) {
	delete reinterpret_cast<CompiledFilter*>(filter);
}
