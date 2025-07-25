#pragma once
#include "Render/Model.h"
#include "Framework/Hashset.h"

// use 16 bit indicies
const int MODEL_BUFFER_INDEX_TYPE_SIZE = sizeof(uint16_t);

class MainVbIbAllocator
{
public:

	void init(uint32_t num_indicies, uint32_t num_verts);
	void print_usage() const;

	int append_to_v_buffer(const uint8_t* data, size_t size);
	int append_to_i_buffer(const uint8_t* data, size_t size);
	int move_append_v_buffer(int ofs, int size);
	int move_append_i_buffer(int ofs, int size);


	struct buffer {
		bufferhandle handle = 0;
		int allocated = 0;

		int used_total = 0;
	
		int tail = 0;	// min element
		int head = 0;	// max element
	};

	buffer vbuffer;
	buffer ibuffer;

private:
	int move_append_buf_shared(int ofs, int size, const char* name, buffer& buf, uint32_t target);
	int append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target);
};
#include "Framework/ConsoleCmdGroup.h"
enum class VaoType {
	Animated,
	Lightmapped
};
class ModelMan
{
public:
	ModelMan();
	void init();
	void add_commands(ConsoleCmdGroup& group);
	void compact_memory();
	void print_usage() const;
	vertexarrayhandle get_vao(VaoType type) { 
		if (type == VaoType::Animated)
			return animated_vao;
		else
			return lightmapped_vao;
	}
	Model* get_error_model() const { return error_model; }
	Model* get_sprite_model() const { return _sprite; }
	Model* get_default_plane_model() const { return defaultPlane; }
	Model* get_light_dome() const { return LIGHT_DOME; }
	Model* get_light_sphere() const { return LIGHT_SPHERE; }
	Model* get_light_cone() const { return LIGHT_CONE; }
	void add_model_to_list(Model* m);
	void remove_model_from_list(Model* m);
private:
	hash_set<Model> all_models;

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

	vertexarrayhandle animated_vao=0;
	vertexarrayhandle lightmapped_vao=0;
	MainVbIbAllocator allocator;
	int cur_mesh_id = 1;

	friend class Model;
	friend class ModelLoadJob;
};

extern ModelMan g_modelMgr;