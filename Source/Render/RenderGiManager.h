#pragma once
#include "DrawLocal.h"

struct R_CubemapVolume {
	glm::mat4x4 transform;
	glm::vec4 extents;
	glm::vec4 bounds_min;
	glm::vec4 bounds_max;
};

struct BakedDdgiInputData {
	std::vector<glm::vec4> offsets;
	IGraphicsTexture* irrad{};
	IGraphicsTexture* depths{};
	std::vector<DdgiVolumeGpu> volumes;
};


class RenderGiManager {
public:
	static RenderGiManager* inst;
	RenderGiManager();

	// updates cubemaps or does raytracing here
	void render_frame_tick();

	void update_ddgi_volumes(const DdgiVolumeGpu& volumes);
	const std::vector<DdgiVolumeGpu>& get_baked_ddgi_volumes() const;
	void set_loaded_ddgi_data(BakedDdgiInputData&& input);
	void bake_ddgi();

	const std::vector<R_CubemapVolume>& get_cubemap_volumes_vector() { return cm_volumes; }
	void update_cubemap_volumes(const std::vector<R_CubemapVolume>& volumes);
	void bake_all_cubemaps() {
		wants_bake_all_cubemaps = true;
	}
	void bake_one_cubemap(int index) {
		bake_these_cubemaps.insert(index);
	}
	void set_cubemaps_from_loading(std::vector<R_CubemapVolume>&& volumes, IGraphicsTexture* cubemaps);


	IGraphicsTexture* get_cubemap_array_texture() { 
		if (runtime_loaded_cubemaps) 
			return runtime_loaded_cubemaps;
		return editable_cubemap_array; 
	}
	IGraphicsBuffer* get_cubemap_volume_buffer() { return cubemap_volume_buffer; }
	int get_num_cubemaps() { return cm_volumes.size(); }



private:

	// ############
	// # CUBEMAPS #
	// ############
	std::vector<R_CubemapVolume> cm_volumes;
	IGraphicsBuffer* cubemap_volume_buffer = nullptr;
	// pre made loaded cubemaps
	IGraphicsTexture* runtime_loaded_cubemaps = nullptr;
	// non compressed form that editor can render into
	IGraphicsTexture* editable_cubemap_array=nullptr;

	bool wants_bake_all_cubemaps = false;
	std::unordered_set<int> bake_these_cubemaps;

	Texture* dummy_temp_cubemap{};

	// ########
	// # DDGI #
	// ########


};
