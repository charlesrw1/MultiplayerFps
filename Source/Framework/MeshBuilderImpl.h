#pragma once
#include "Framework/Util.h"
class MeshBuilder;
class IGraphicsBuffer;
class IGraphicsVertexInput;
class MeshBuilderDD
{
public:
	void free();
	void draw(uint32_t type);
	void init_from(MeshBuilder& mb);

	// can also use these instead
	static const uint32_t TRIANGLES;
	static const uint32_t LINES;

	IGraphicsBuffer* vbo = nullptr;
	IGraphicsBuffer* ebo = nullptr;
	IGraphicsVertexInput* vao = nullptr;
	int num_indicies = 0;
};