#if 0
#include <meshoptimizer.h>
#include <vector>
#include "Model.h"
#include "glad/glad.h"
#include "Meshlet.h"
using std::vector;

const static uint32_t MAX_CHUNK_VERTS = 64;
const static uint32_t MAX_CHUNK_TRIS = 256;

extern bool use_32_bit_indicies;
// looked at github/lighthugger for inspo
Chunked_Model* get_chunked_mod(const char* filename)
{
	assert(use_32_bit_indicies);

	const int ELEMENT_SIZE = 4;
	const int POS_STRIDE = 12;

	Model* m = mods.find_or_load(filename);

	Chunked_Model* chunked_mesh = new Chunked_Model;
	chunked_mesh->model = m;

	Mesh* mesh = &m->mesh;

	for (auto& part : mesh->parts) {
		char* index_data = mesh->data.indicies.data() + part.element_offset;
		glm::vec3* pos_data = (glm::vec3*)(mesh->data.buffers[0].data()) + part.base_vertex;
		int num_indicies = part.element_count;
		int num_verts = mesh->data.buffers[0].size() / POS_STRIDE;

		size_t max_meshlets = meshopt_buildMeshletsBound(part.element_count, MAX_CHUNK_VERTS, MAX_CHUNK_TRIS);
		vector<meshopt_Meshlet> meshlets(max_meshlets);
		vector<uint32_t> meshlet_indicies(max_meshlets * MAX_CHUNK_VERTS);
		vector<unsigned char> micro_indices(max_meshlets * MAX_CHUNK_TRIS * 3);
		size_t num_meshlets = 0;
		num_meshlets = meshopt_buildMeshlets(
			meshlets.data(),
			meshlet_indicies.data(),
			micro_indices.data(),
			(uint32_t*)index_data,
			part.element_count,
			(float*)pos_data,
			num_verts,
			POS_STRIDE,
			MAX_CHUNK_VERTS,
			MAX_CHUNK_TRIS,
			0.5);
		const meshopt_Meshlet& last = meshlets[num_meshlets - 1];
		size_t num_meshlet_indicies = last.vertex_offset + last.vertex_count;
		size_t num_micro_indicies = last.triangle_offset + last.triangle_count * 3;

		meshlet_indicies.resize(last.vertex_offset + last.vertex_count);
		micro_indices.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
		meshlets.resize(num_meshlets);
		std::vector<meshopt_Bounds> meshlet_bounds(num_meshlets);

		for (size_t i = 0; i < num_meshlets; i++) {
		    auto meshlet = meshlets[i];
		    meshlet_bounds[i] = meshopt_computeMeshletBounds(
		        &meshlet_indicies[meshlet.vertex_offset],
		        &micro_indices[meshlet.triangle_offset],
		        meshlet.triangle_count,
		        (float*)pos_data,
		        num_verts,
		        POS_STRIDE
		    );

		    // This means that I've done something wrong.
		    if (meshlet_bounds[i].radius == 0) {
				assert(!"0 radius meshlet");
		    }
		}

		std::vector<Chunk> final_meshlets(num_meshlets);
		for (size_t i = 0; i < num_meshlets; i++) {
		    auto meshlet = meshlets[i];
		    meshopt_Bounds bounds = meshlet_bounds[i];
			final_meshlets[i].bounding_sphere = glm::vec4(
				bounds.center[0],
				bounds.center[1],
				bounds.center[2],
				bounds.radius
			);
			final_meshlets[i].cone_apex = glm::vec4(
				bounds.cone_apex[0],
				bounds.cone_apex[1],
				bounds.cone_apex[2],
				0.f
			);
			final_meshlets[i].cone_axis_cutoff = glm::vec4(
				bounds.cone_axis[0],
		        bounds.cone_axis[1],
				bounds.cone_axis[2],
				bounds.cone_cutoff
			);

			const uint32_t* meshlet_vertices = &meshlet_indicies[meshlet.vertex_offset];
			const uint8_t* meshlet_triangles = &micro_indices[meshlet.triangle_offset];
			vector<uint32_t> the_actual_fucking_indicies(meshlet.triangle_count * 3);
			for (int i = 0; i < the_actual_fucking_indicies.size(); i++)
				the_actual_fucking_indicies[i] = meshlet_vertices[meshlet_triangles[i]];

			final_meshlets[i].index_count = meshlet.triangle_count * 3;
			final_meshlets[i].index_offset = chunked_mesh->indicies.size() * sizeof(uint32_t);
			// append indicies
			chunked_mesh->indicies.insert(
				chunked_mesh->indicies.end(),
				the_actual_fucking_indicies.begin(),
				the_actual_fucking_indicies.end()
			);
		}

		Subpart_ext si;
		si.chunk_count = final_meshlets.size();
		si.chunk_start = chunked_mesh->chunks.size();

		chunked_mesh->chunks.insert(
			chunked_mesh->chunks.end(),
			final_meshlets.begin(),
			final_meshlets.end()
		);

		chunked_mesh->parts_ext.push_back(si);
	}


	uint32_t vao, index_buffer;

	uint32_t pos_buffer = mods.global_vertex_buffers[1].attributes[0].handle;
	uint32_t normal_buffer = mods.global_vertex_buffers[1].attributes[2].handle;
	uint32_t uv_buffer = mods.global_vertex_buffers[1].attributes[1].handle;


	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
		chunked_mesh->indicies.size() * sizeof(uint32_t), 
		chunked_mesh->indicies.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, pos_buffer);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3,GL_FLOAT,
			false, 12, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3,GL_SHORT,
			true, 6, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2,GL_FLOAT,
			false, 8, (void*)0);

	glBindVertexArray(0);

	chunked_mesh->vao = vao;
	chunked_mesh->indicies_buffer = index_buffer;

	return chunked_mesh;
}
#endif