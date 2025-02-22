#pragma once
#include "Framework/Util.h"
class MeshBuilder;
class MeshBuilderDD
{
public:
	void free();
	void draw(uint32_t type);
	void init_from(MeshBuilder& mb);

	// can also use these instead
	static const uint32_t TRIANGLES;
	static const uint32_t LINES;

	uint32_t VAO = 0;
	uint32_t VBO = 0;
	uint32_t EBO = 0;
	int num_indicies = 0;
};