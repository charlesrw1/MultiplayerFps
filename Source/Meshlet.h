#pragma once
#include <cstdint>
#include "glm/glm.hpp"
#include <vector>
using std::vector;
using namespace glm;

struct Chunk
{
	vec4 cone_apex;
    vec4 cone_axis_cutoff;
    vec4 bounding_sphere;

	int index_offset;	// offset into model's index buffer
    int index_count;	// number of indicies in this chunk, max = 64*3
};

struct Subpart_ext
{
	uint32_t chunk_count;
	uint32_t chunk_start;
};

struct Chunked_Model
{
	uint32_t vao;
	uint32_t indicies_buffer;
	vector<uint32_t> indicies;
	vector<Chunk> chunks;
	vector<Subpart_ext> parts_ext;
	
	Model* model;
};


Chunked_Model* get_chunked_mod(const char* filename);