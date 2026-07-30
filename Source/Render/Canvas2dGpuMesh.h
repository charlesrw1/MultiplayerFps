#pragma once
class MeshBuilder;
class IGraphicsBuffer;
class IGraphicsVertexInput;

// GPU-upload wrapper for 2d geometry (transient per-frame arena, or one
// Canvas2dVertexArray's persistent buffer). Deliberately separate from MeshBuilderDD
// (Source/Framework/MeshBuilderImpl.h), which stays reserved for the unrelated 3D
// debug-draw path -- see the Canvas2d design doc for why the two shouldn't share code.
//
// Unlike MeshBuilderDD, upload_from() grows the underlying buffers in place via
// sub_upload when new data fits current capacity, instead of a forced full reupload
// every call -- matters for a persistent VertexArray that's re-uploaded after a small
// edit, not for the transient per-frame arena (which is 100% new data every frame
// anyway, so it always takes the full-upload path).
class Canvas2dGpuMesh
{
public:
	void release();
	void upload_from(MeshBuilder& mb);

	IGraphicsBuffer* vbo = nullptr;
	IGraphicsBuffer* ebo = nullptr;
	IGraphicsVertexInput* vao = nullptr;
	int num_indices = 0;
	int vbo_capacity_bytes = 0;
	int ebo_capacity_bytes = 0;
};
