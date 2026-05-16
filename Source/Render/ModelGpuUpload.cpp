// ModelGpuUpload.cpp — GPU buffer allocation, vertex/index upload, ModelMan init & default models
// Split from Model.cpp to keep file sizes under 600 LOC.

#include "Model.h"
#include "ModelManager.h"

#include "Memory.h"
#include <vector>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"
#include "Texture.h"
#include "Framework/Files.h"
#include "Framework/Config.h"
#include <algorithm>

#include "Render/MaterialPublic.h"
#include "Render/MaterialLocal.h"
#include "DrawLocal.h"
#include "IGraphicsDevice.h"

static const int STATIC_VERTEX_SIZE = 4'000'000;
static const int STATIC_INDEX_SIZE  = 6'000'000;

void MainVbIbAllocator::init(uint32_t num_indicies, uint32_t num_verts) {
	ASSERT(num_indicies > 0 && num_verts > 0);

	vbuffer.alloc.init_clear(sizeof(ModelVertex) * STATIC_VERTEX_SIZE);

	CreateBufferArgs args;
	args.flags = BUFFER_USE_AS_VB;
	args.size = sizeof(ModelVertex) * STATIC_VERTEX_SIZE;
	vbuffer.ptr = gfx().create_buffer(args);

	const int index_size = MODEL_BUFFER_INDEX_TYPE_SIZE;
	ibuffer.alloc.init_clear(index_size * STATIC_INDEX_SIZE);

	args.flags = BUFFER_USE_AS_IB;
	args.size = index_size * STATIC_INDEX_SIZE;
	ibuffer.ptr = gfx().create_buffer(args);
}

gpuAllocSpan MainVbIbAllocator::append_to_v_buffer(const uint8_t* data, size_t size) {
	ASSERT(data != nullptr && size > 0);
	return append_buf_shared(data, size, "Vertex", vbuffer, GL_ARRAY_BUFFER);
}
gpuAllocSpan MainVbIbAllocator::append_to_i_buffer(const uint8_t* data, size_t size) {
	ASSERT(data != nullptr && size > 0);
	return append_buf_shared(data, size, "Index", ibuffer, GL_ELEMENT_ARRAY_BUFFER);
}

gpuAllocSpan MainVbIbAllocator::append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf,
                                                   uint32_t target) {
	ASSERT(data != nullptr);
	ASSERT(size > 0);
	ASSERT(name != nullptr);

	auto out_of_memory = [&]() {
		// sys_print(Error, "%s buffer overflow %d/%d !!!\n", name, int(size + buf.used_total), int(buf.allocated));
		std::fflush(stdout);
		std::abort();
	};

	// fixme
	// also for different vertex layouts etc
	int align_size = MODEL_BUFFER_INDEX_TYPE_SIZE;
	if (target == GL_ARRAY_BUFFER)
		align_size = sizeof(ModelVertex);

	const gpuAllocSpan my_ptr = buf.alloc.allocate(size, align_size);
	if (my_ptr.size == 0) // fixme
		out_of_memory();

	buf.ptr->sub_upload(data, size, my_ptr.aligned_start);

	return my_ptr;
}

void MainVbIbAllocator::print_usage() const {
	ASSERT(true); // always callable
	auto print_facts = [](const char* name, const buffer& b, int element_size) {
		// float used_percentage = 1.0;
		// if (b.allocated > 0)
		//	used_percentage = (double)b.used_total / (double)b.allocated;
		// used_percentage *= 100.0;
		//
		// int used_elements = b.used_total / element_size;
		// int allocated_elements = b.allocated / element_size;
		// sys_print(Info, "	%s: %d/%d (%.1f%%) (bytes:%d) (%d:%d)\n", name, used_elements, allocated_elements,
		// use_percentage, b.used_total, b.tail, b.head);
	};
	sys_print(Info, "MainVbIbAllocator::print_usage\n");

	print_facts("IndexBuffer", ibuffer, MODEL_BUFFER_INDEX_TYPE_SIZE);
	print_facts("VertexBuffer", vbuffer, sizeof(ModelVertex));
}

void ModelMan::init() {
	ASSERT(gfx_is_initialized());
	allocator.init(STATIC_INDEX_SIZE, STATIC_VERTEX_SIZE);

	{
		using gvat = GraphicsVertexAttribType;
		const int stride = sizeof(ModelVertex);

		CreateVertexInputArgs args;
		args.index = allocator.ibuffer.ptr;
		args.vertex = allocator.vbuffer.ptr;
		args.index_type = VertexInputIndexType::uint16;
		auto animated_layout = {
			VertexLayout(POSITION_LOC, 3, gvat::float32, stride, offsetof(ModelVertex, pos)),
			VertexLayout(UV_LOC, 2, gvat::float32, stride, offsetof(ModelVertex, uv)),
			VertexLayout(NORMAL_LOC, 3, gvat::i16_normalized, stride, offsetof(ModelVertex, normal[0])),
			VertexLayout(TANGENT_LOC, 3, gvat::u16, stride, offsetof(ModelVertex, tangent[0])),
			VertexLayout(JOINT_LOC, 4, gvat::u8, stride, offsetof(ModelVertex, color[0])),
			VertexLayout(WEIGHT_OR_COLOR_LOC, 4, gvat::u8_normalized, stride, offsetof(ModelVertex, color2[0])),
		};
		args.layout = animated_layout;
		animated_vertex_input = gfx().create_vertex_input(args);
	}
	{
		using gvat = GraphicsVertexAttribType;
		const int stride = sizeof(ModelVertex);

		CreateVertexInputArgs args;
		args.index = allocator.ibuffer.ptr;
		args.vertex = allocator.vbuffer.ptr;
		args.index_type = VertexInputIndexType::uint16;
		auto lightmapped_layout = {
			VertexLayout(POSITION_LOC, 3, gvat::float32, stride, offsetof(ModelVertex, pos)),
			VertexLayout(UV_LOC, 2, gvat::float32, stride, offsetof(ModelVertex, uv)),
			VertexLayout(NORMAL_LOC, 3, gvat::i16_normalized, stride, offsetof(ModelVertex, normal[0])),
			VertexLayout(TANGENT_LOC, 3, gvat::u16, stride, offsetof(ModelVertex, tangent[0])),
			VertexLayout(LIGHTMAPCOORD_LOC, 2, gvat::u16_normalized, stride, offsetof(ModelVertex, color[0])),
			VertexLayout(WEIGHT_OR_COLOR_LOC, 4, gvat::u8_normalized, stride, offsetof(ModelVertex, color2[0])),
		};
		args.layout = lightmapped_layout;
		lightmapped_vertex_input = gfx().create_vertex_input(args);
	}

	create_default_models();
	auto& a = g_assets;
	LIGHT_CONE = a.find<Model>("eng/LIGHT_CONE.cmdl").get();
	LIGHT_SPHERE = a.find<Model>("eng/LIGHT_SPHERE.cmdl").get();
	LIGHT_DOME = a.find<Model>("eng/LIGHT_DOME.cmdl").get();

	if (!LIGHT_CONE || !LIGHT_SPHERE || !LIGHT_DOME)
		Fatalf("!!! ModelMan::init: couldn't load default LIGHT_x volumes (used for gbuffer lighting)\n");
}

void ModelMan::create_default_models() {
	ASSERT(error_model == nullptr); // should only be called once during init
	error_model = g_assets.find<Model>("eng/question.cmdl").get();
	if (!error_model)
		Fatalf("couldnt load error model (question.cmdl)\n");
	defaultPlane = g_assets.find<Model>("eng/plane.cmdl").get();
	if (!defaultPlane)
		Fatalf("couldnt load defaultPlane model\n");

	_sprite = new Model;
	_sprite->aabb = Bounds(glm::vec3(-0.5), glm::vec3(0.5));
	_sprite->bounding_sphere = bounds_to_sphere(_sprite->aabb);
	_sprite->materials.push_back(imaterials->get_fallback_sptr());
	{
		ModelVertex corners[4];
		corners[0].pos = glm::vec3(0.5, 0.5, 0.0);
		corners[0].uv = glm::vec2(1.0, 0.0);
		corners[0].normal[0] = 0;
		corners[0].normal[1] = 0;
		corners[0].normal[2] = INT16_MAX;

		corners[1].pos = glm::vec3(-0.5, 0.5, 0.0);
		corners[1].uv = glm::vec2(0.0, 0.0);
		corners[1].normal[0] = 0;
		corners[1].normal[1] = 0;
		corners[1].normal[2] = INT16_MAX;

		corners[2].pos = glm::vec3(-0.5, -0.5, 0.0);
		corners[2].uv = glm::vec2(0.0, 1.0);
		corners[2].normal[0] = 0;
		corners[2].normal[1] = 0;
		corners[2].normal[2] = INT16_MAX;

		corners[3].pos = glm::vec3(0.5, -0.5, 0.0);
		corners[3].uv = glm::vec2(1.0, 1.0);
		corners[3].normal[0] = 0;
		corners[3].normal[1] = 0;
		corners[3].normal[2] = INT16_MAX;

		_sprite->data.verts.push_back(corners[0]);
		_sprite->data.verts.push_back(corners[1]);
		_sprite->data.verts.push_back(corners[2]);
		_sprite->data.verts.push_back(corners[3]);
		_sprite->data.indicies.push_back(0);
		_sprite->data.indicies.push_back(1);
		_sprite->data.indicies.push_back(2);
		_sprite->data.indicies.push_back(0);
		_sprite->data.indicies.push_back(2);
		_sprite->data.indicies.push_back(3);

		Submesh sm;
		sm.base_vertex = 0;
		sm.element_count = 6;
		sm.element_offset = 0;
		sm.material_idx = 0;
		sm.vertex_count = 4;
		MeshLod lod;
		lod.end_percentage = 1.0;
		lod.part_count = 1;
		lod.part_ofs = 0;

		_sprite->parts.push_back(sm);
		_sprite->lods.push_back(lod);
		upload_model(_sprite);

		g_assets.install_system_asset(_sprite, "_SPRITE");
	}
}

// Uploads the model's vertex and index data to the GPU
// and sets the model's ptrs/offsets into the global vertex buffer
bool ModelMan::upload_model(Model* mesh) {
	ASSERT(mesh != nullptr);
	ASSERT(all_models.find(mesh) == nullptr);
	all_models.insert(mesh);

	if (mesh->uid == 0)
		mesh->uid = cur_mesh_id++;
	else {
		sys_print(Debug, "model reloaded: %s\n", mesh->get_name().c_str());
		BuildSceneData_CpuFast::inst->rebuild_models(); // force rebuild models...
	}

	// sys_print(Debug, "uploading mode: %s\n", mesh->get_name().c_str());

	if (mesh->parts.size() == 0) {
		sys_print(Warning, "ModelMan::upload_model: model has not parts (%s)\n", mesh->get_name().c_str());
		return false;
	}

	size_t indiciesbufsize{};
	const uint8_t* const ibufferdata = mesh->data.get_index_data(&indiciesbufsize);
	mesh->index_alloc_ptr = allocator.append_to_i_buffer(
		ibufferdata, indiciesbufsize); // dont divide by sizeof(uint16_2), this is an pointer

	size_t vertbufsize{};
	const uint8_t* const v_bufferdata = mesh->data.get_vertex_data(&vertbufsize);
	mesh->vertex_alloc_ptr = allocator.append_to_v_buffer(v_bufferdata, vertbufsize);
	// mesh->merged_vert_offset /= sizeof(ModelVertex);

	bool has_transparent = false;
	for (int i = 0; i < mesh->parts.size(); i++) {
		auto& part = mesh->parts.at(i);
		auto mat = mesh->materials.at(part.get_material_idx_to_use()).get();
		if (mat && mat->impl && mat->impl->get_master_impl() && mat->impl->get_master_impl()->is_translucent()) {
			has_transparent = true;
			part.set_material_transparent();
		}
	}
	mesh->has_any_transparent_materials = has_transparent;

	return true;
}

void ModelMan::remove_model_from_list(Model* m) {
	ASSERT(m != nullptr);

	// Null any fast-path render cache entries keyed on this model before the
	// pointer becomes invalid, so the renderer never dereferences a freed Model.
	if (BuildSceneData_CpuFast::inst)
		BuildSceneData_CpuFast::inst->on_model_removed(m);

	allocator.ibuffer.alloc.free(m->index_alloc_ptr);
	allocator.vbuffer.alloc.free(m->vertex_alloc_ptr);
	m->index_alloc_ptr = {};
	m->vertex_alloc_ptr = {};

	all_models.remove(m);
	ASSERT(!all_models.find(m));
}
