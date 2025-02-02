#pragma once
#include "Render/Model.h"

// use 16 bit indicies
const int MODEL_BUFFER_INDEX_TYPE_SIZE = sizeof(uint16_t);

class MainVbIbAllocator
{
public:

	void init(uint32_t num_indicies, uint32_t num_verts);
	void print_usage() const;

	void append_to_v_buffer(const uint8_t* data, size_t size);
	void append_to_i_buffer(const uint8_t* data, size_t size);


	struct buffer {
		bufferhandle handle = 0;
		uint32_t allocated = 0;
		uint32_t used = 0;
	};

	buffer vbuffer;
	buffer ibuffer;

private:
	void append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target);
};

class ModelMan
{
public:
	static ModelMan& get() {
		static ModelMan inst;
		return inst;
	}

	void init();

	void compact_memory();
	void print_usage() const;

	vertexarrayhandle get_vao(bool animated) {
		return animated_vao;
	}

	Model* get_error_model() const { return error_model; }
	Model* get_sprite_model() const { return _sprite; }
	Model* get_default_plane_model() const { return defaultPlane; }

	Model* get_light_dome() const { return LIGHT_DOME; }
	Model* get_light_sphere() const { return LIGHT_SPHERE; }
	Model* get_light_cone() const { return LIGHT_CONE; }

private:

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

	vertexarrayhandle animated_vao;
	//vertexarrayhandle static_vao;
	MainVbIbAllocator allocator;

	uint32_t cur_mesh_id = 0;

	friend class Model;
	friend class ModelLoadJob;
};