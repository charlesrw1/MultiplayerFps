#pragma once
#include <cstdint>
#include "glm/glm.hpp"
#include <vector>
using std::vector;
using namespace glm;

struct Chunk
{
    vec4 bounding_sphere;
	uint32_t cone;
	uint32_t count; // number of indicies in this chunk, max = 64*3
	uint32_t offset; // offset into model's index buffer
	uint32_t padding;
};

// Meshlet global_meshlet_table[]
// struct Submesh {
//	..
// meshlet_ofs = 0;
// meshlet_count = 0;
// ..
// };
// 
// uint global_index_buffer[]
// Vertex global_vertex_buffer[]
// 

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

//Chunked_Model* get_chunked_mod(const char* filename);