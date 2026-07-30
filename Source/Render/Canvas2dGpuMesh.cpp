#include "Render/Canvas2dGpuMesh.h"
#include "Framework/MeshBuilder.h"
#include "Render/IGraphicsDevice.h"

void Canvas2dGpuMesh::release() {
	safe_release(vao);
	safe_release(vbo);
	safe_release(ebo);
	num_indices = 0;
	vbo_capacity_bytes = 0;
	ebo_capacity_bytes = 0;
}

void Canvas2dGpuMesh::upload_from(MeshBuilder& mb) {
	const int vb_size = (int)(mb.get_v().size() * sizeof(MbVertex));
	const int ib_size = (int)(mb.get_i().size() * sizeof(uint32_t));

	if (!vao) {
		CreateBufferArgs vb_args;
		vb_args.flags = (GraphicsBufferUseFlags)(BUFFER_USE_AS_VB | BUFFER_USE_DYNAMIC);
		vbo = gfx().create_buffer(vb_args);

		CreateBufferArgs ib_args;
		ib_args.flags = (GraphicsBufferUseFlags)(BUFFER_USE_AS_IB | BUFFER_USE_DYNAMIC);
		ebo = gfx().create_buffer(ib_args);

		const VertexLayout layout[] = {
			VertexLayout(0, 3, GraphicsVertexAttribType::float32, sizeof(MbVertex), 0),
			VertexLayout(1, 4, GraphicsVertexAttribType::u8_normalized, sizeof(MbVertex), 3 * sizeof(float)),
			VertexLayout(2, 2, GraphicsVertexAttribType::float32, sizeof(MbVertex), 3 * sizeof(float) + sizeof(Color32)),
		};
		CreateVertexInputArgs vargs;
		vargs.vertex = vbo;
		vargs.index = ebo;
		vargs.layout = layout;
		vargs.index_type = VertexInputIndexType::uint32;
		vao = gfx().create_vertex_input(vargs);
	}

	if (vb_size > vbo_capacity_bytes) {
		vbo->upload(mb.get_v().data(), vb_size);
		vbo_capacity_bytes = vb_size;
	} else if (vb_size > 0) {
		vbo->sub_upload(mb.get_v().data(), vb_size, 0);
	}

	if (ib_size > ebo_capacity_bytes) {
		ebo->upload(mb.get_i().data(), ib_size);
		ebo_capacity_bytes = ib_size;
	} else if (ib_size > 0) {
		ebo->sub_upload(mb.get_i().data(), ib_size, 0);
	}

	num_indices = (int)mb.get_i().size();
}
