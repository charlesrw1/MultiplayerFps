#include "RmlUiRenderInterface.h"
#include "IGraphicsDevice.h"
#include "Texture.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "../Shaders/ShaderBufferShared.txt"
#include "Framework/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstddef>

RmlUiRenderInterface::RmlUiRenderInterface() {
	shader_program = gfx().create_shader_vert_frag("RmlUiV.txt", "RmlUiF.txt");

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
}

RmlUiRenderInterface::~RmlUiRenderInterface() {
	safe_release(white_texture);
	safe_release(shader_program);
	for (auto& [handle, tex] : texture_map)
		if (tex.owns_texture)
			safe_release(tex.tex);
	for (auto& [handle, geo] : geometry_map) {
		safe_release(geo.vao);
		safe_release(geo.vbo);
		safe_release(geo.ebo);
	}
}

void RmlUiRenderInterface::begin_frame(int viewport_w, int viewport_h) {
	viewport_width = viewport_w;
	viewport_height = viewport_h;
	// Orthographic projection matching pixel coordinates, origin top-left.
	projection = glm::ortho(0.f, (float)viewport_w, (float)viewport_h, 0.f);
}

void RmlUiRenderInterface::end_frame() {
	gfx().disable_scissor();
}

Rml::CompiledGeometryHandle RmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	CompiledGeometry geo;

	CreateBufferArgs vb_args;
	vb_args.flags = (GraphicsBufferUseFlags)BUFFER_USE_AS_VB;
	geo.vbo = gfx().create_buffer(vb_args);
	geo.vbo->upload(vertices.data(), (int)(vertices.size() * sizeof(Rml::Vertex)));

	CreateBufferArgs ib_args;
	ib_args.flags = (GraphicsBufferUseFlags)BUFFER_USE_AS_IB;
	geo.ebo = gfx().create_buffer(ib_args);
	geo.ebo->upload(indices.data(), (int)(indices.size() * sizeof(int)));

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

	geo.index_count = (int)indices.size();
	Rml::CompiledGeometryHandle handle = next_geometry_handle++;
	geometry_map[handle] = geo;
	return handle;
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;

	IGraphicsTexture* tex = white_texture;
	if (texture != 0) {
		auto tex_it = texture_map.find(texture);
		if (tex_it != texture_map.end())
			tex = tex_it->second.tex;
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
	pcv.Projection = projection;
	pcv.Translation = { translation.x, translation.y };
	gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

	gfx().bind_texture(0, tex);
	gfx().draw_elements(GraphicsPrimitiveType::Triangles, it->second.index_count, VertexInputIndexType::uint32, 0);
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;
	safe_release(it->second.vao);
	safe_release(it->second.vbo);
	safe_release(it->second.ebo);
	geometry_map.erase(it);
}

Rml::TextureHandle RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	Texture* t = Texture::force_load_for_ui(source);
	if (!t || !t->gpu_ptr) {
		sys_print(Warning, "RmlUiRenderInterface::LoadTexture failed for source '%s'\n", source.c_str());
		texture_dimensions = { 0, 0 };
		return 0;
	}
	const glm::ivec2 size = t->get_size();
	texture_dimensions = { size.x, size.y };

	Rml::TextureHandle handle = next_texture_handle++;
	texture_map[handle] = { t->gpu_ptr, false };
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
	texture_map[handle] = { tex, true };
	return handle;
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
	auto it = texture_map.find(texture);
	if (it == texture_map.end())
		return;
	if (it->second.owns_texture)
		safe_release(it->second.tex);
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
