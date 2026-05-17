#pragma once
#include "Render/Model.h"
#include "Render/IGraphicsDevice.h"
#include "Framework/Hashset.h"
#include "GpuAllocator.h"
#include <vector>

// use 16 bit indicies
const int MODEL_BUFFER_INDEX_TYPE_SIZE = sizeof(uint16_t);
// Index type used for model element buffers. Mirrors MODEL_INDEX_TYPE_GL = GL_UNSIGNED_SHORT.
constexpr VertexInputIndexType MODEL_INDEX_TYPE = VertexInputIndexType::uint16;
class IGraphicsBuffer;
class MainVbIbAllocator
{
public:
	void init(uint32_t num_indicies, uint32_t num_verts);
	void print_usage() const;

	gpuAllocSpan append_to_v_buffer(const uint8_t* data, size_t size);
	gpuAllocSpan append_to_i_buffer(const uint8_t* data, size_t size);

	struct buffer
	{
		IGraphicsBuffer* ptr = nullptr;
		gpuSpanAllocator alloc;

		int get_allocated() const { return alloc.get_init_sized(); }
	};

	buffer vbuffer;
	buffer ibuffer;

private:
	gpuAllocSpan append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target);
};
#include "Framework/ConsoleCmdGroup.h"
enum class VaoType
{
	Animated,
	Lightmapped
};
class IGraphicsVertexInput;
class ModelMan
{
public:
	ModelMan();
	void init();
	void add_commands(ConsoleCmdGroup& group);
	void compact_memory();
	void print_usage() const;

	IGraphicsVertexInput* get_vao_ptr(VaoType type) {
		if (type == VaoType::Animated)
			return animated_vertex_input;
		else
			return lightmapped_vertex_input;
	}

	Model* get_error_model() const { return error_model; }
	Model* get_sprite_model() const { return _sprite; }
	Model* get_default_plane_model() const { return defaultPlane; }
	Model* get_light_dome() const { return LIGHT_DOME; }
	Model* get_light_sphere() const { return LIGHT_SPHERE; }
	Model* get_light_cone() const { return LIGHT_CONE; }

	void remove_model_from_list(Model* m);

	const hash_set<Model>& get_all_models() const { return all_models; }

	// -----------------------------------------------------------------------
	// Dynamic (procedural) model API
	// -----------------------------------------------------------------------

	/// Build and upload a renderable model from procedural geometry.
	/// The model is owned by ModelMan and tracked in live_dynamic_models.
	/// Caller must eventually pass the returned pointer to free_dynamic_model(),
	/// or hold it via DynamicModelUniquePtr for automatic cleanup.
	/// The model gets a single LOD, single submesh, and the engine fallback
	/// material; swap the material via get_material()/materials after creation
	/// if needed.
	Model* create_dynamic_model(const ModelBuilder& builder,
	                            const std::string& debug_name = "dynamic");

	/// Release a dynamic model created with create_dynamic_model().
	/// Frees GPU vertex/index allocations and deletes the Model object.
	/// The pointer is invalid after this call.
	void free_dynamic_model(Model* m);

	/// Re-upload a dynamic model in-place with new geometry from builder.
	/// Existing GPU allocations are freed and replaced; the Model pointer stays valid.
	/// All submesh/material slots are rebuilt from the builder's submesh entries.
	void refresh_dynamic_model(Model* m, const ModelBuilder& builder);

	/// Number of live dynamic models.  Useful for leak detection in tests.
	int get_num_dynamic_models() const { return (int)live_dynamic_models.size(); }

private:
	hash_set<Model> all_models;

	/// Owns all live dynamic models.  Indexed by Model* for O(n) lookup
	/// on free (dynamic model counts are expected to be small).
	std::vector<std::unique_ptr<Model>> live_dynamic_models;

	// Used for gbuffer lighting
	Model* LIGHT_DOME = nullptr;
	Model* LIGHT_SPHERE = nullptr;
	Model* LIGHT_CONE = nullptr;

	Model* error_model = nullptr;
	Model* _sprite = nullptr;
	Model* defaultPlane = nullptr;

	void create_default_models();
	void set_v_attributes();
	bool upload_model(Model* m);
	static void populate_model_from_builder(Model* m, const ModelBuilder& builder);

	IGraphicsVertexInput* animated_vertex_input = nullptr;
	IGraphicsVertexInput* lightmapped_vertex_input = nullptr;

	MainVbIbAllocator allocator;
	int cur_mesh_id = 1;

	friend class Model;
	friend class ModelLoadJob;
};

extern ModelMan g_modelMgr;