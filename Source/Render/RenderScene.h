#pragma once
#include "DrawPublic.h"

// All the renderable types
#include "RenderObj.h"
#include "Render/Render_Decal.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "Render/Render_Volumes.h"

#include "Framework/FreeList.h"

#include "../Shaders/SharedGpuTypes.txt"

#include "DrawTypedefs.h"

#include <cstdint>

#include "Render/MaterialLocal.h"


struct Render_Box_Cubemap
{
	vec3 boxmin;
	vec3 boxmax;
	vec3 probe_pos = vec3(0.f);
	int priority = 0;
	bool found_probe_flag = false;
	int id = -1;
};


// represents a singular call to glDrawElements() with same state and mesh, batchable
struct Mesh_Batch
{
	uint32_t first = 0;	// indexes into pass.sorted_list
	uint32_t count = 0;

	uint32_t shader_index = 0;	// indexes into shader_list[]
	const MaterialInstanceLocal* material = nullptr;
};

// represents multiple Mesh_Batch calls packaged into one glMultidrawIndirect()
struct Multidraw_Batch
{
	uint32_t first = 0;
	uint32_t count = 0;
};

// represents one draw call of a mesh with a material and various state, sorted and put into Mesh_Batch's
struct draw_call_key {
	draw_call_key() {
		shader = blending = backface = texture = vao = mesh = 0;
		distance = 0;
		layer = 0;
	}

	// lowest
	uint64_t distance : 14;
	uint64_t mesh : 14;
	uint64_t vao : 3;
	uint64_t texture : 14;
	uint64_t backface : 1;
	uint64_t blending : 3;
	uint64_t shader : 12;
	uint64_t layer : 3;
	// highest

	// :)
	uint64_t as_uint64() const {
		return *(reinterpret_cast<const uint64_t*>(this));
	}
};
static_assert(sizeof(draw_call_key) == 8, "key needs 8 bytes");

struct Pass_Object
{
	draw_call_key sort_key;
	const MaterialInstanceLocal* material = nullptr;
	handle<Render_Object> render_obj{};	// entity instance
	uint16_t submesh_index = 0;		// what submesh am i
	uint16_t lod_index = 0;
	uint32_t hl_obj_index{};
	uint32_t batch_idx = 0;
};

// in the end: want a flat list of batches that are merged with neighbors
enum class pass_type
{
	OPAQUE,			// front to back sorting
	TRANSPARENT,	// back to front sorting
	DEPTH			// front to back sorting, ignores material textures unless alpha tested
};

typedef int passobj_handle;

// A render_pass is one collection of POSSIBLE draw calls
// A render_list is created from a pass which is the actual draw calls to submit along with extra buffers to facilitate it
class Render_Scene;
class Render_Pass
{
public:
	Render_Pass(pass_type type);

	void make_batches(Render_Scene& scene);

	void add_object(
		const Render_Object& proxy,
		handle<Render_Object> handle,
		const MaterialInstanceLocal* material,
		uint32_t camera_dist,
		uint32_t submesh,
		uint32_t lod,
		uint32_t layer, bool is_editor_mode);

	draw_call_key create_sort_key_from_obj(
		const Render_Object& proxy,
		const MaterialInstanceLocal* material,
		uint32_t camera_dist,
		uint32_t submesh,
		uint32_t layer, bool is_editor_mode);

	void clear() {
		objects.clear();
		high_level_objects_in_pass.clear();
	}

	const pass_type type{};					// modifies batching+sorting logic

	// all Render_Objects in the pass
	// there will likely be multiple Pass_Objects from 1 R_O like multiple submeshes and LODs all get added
	// this is the array that gets frustum + occlusion culled to make the final Render_Lists structure
	// this means static objects can cache LODs
	std::vector<handle<Render_Object>> high_level_objects_in_pass;
	std::vector<Pass_Object> objects;		// geometry + material id + object id
	std::vector<Mesh_Batch> mesh_batches;	// glDrawElementsIndirect()
	std::vector<Multidraw_Batch> batches;	// glMultiDrawElementsIndirect()
};

// RenderObject internal data
struct ROP_Internal
{
	Render_Object proxy;
	glm::mat4 inv_transform;
	glm::vec4 bounding_sphere_and_radius;
};

// RenderLight internal data
struct RL_Internal
{
	Render_Light light;
	// stuff like shadowmap indicies etc.
	int shadow_array_index = -1;
};

// kinda cursed, render decal maintains a handle for a render object ...
struct RDecal_Internal
{
	Render_Decal decal;
	handle<Render_Object> object;
};


struct Render_Lists
{
	void init(uint32_t drawidsz, uint32_t instbufsz);

	void build_from(Render_Pass& src,
		Free_List<ROP_Internal>& proxy_list);

	uint32_t indirect_drawid_buf_size = 0;
	uint32_t indirect_instance_buf_size = 0;

	// commands to input to glMultiDrawElementsIndirect
	std::vector<gpu::DrawElementsIndirectCommand> commands;
	bufferhandle gpu_command_list = 0;
	// command_count is the number of commands per glMultiDrawElementsIndirect command
	// for now its just set to batches[i].count in the Render_Pass
	// when calling glMDEI, the offset into commands is the summation of previous command counts essentially
	// it works like an indirection into commands
	std::vector<uint32_t> command_count;
	bufferhandle gpu_command_count = 0;

	// maps the gl_DrawID to submesh material (dynamically uniform for bindless)
	bufferhandle gldrawid_to_submesh_material;
	// maps gl_baseinstance + gl_instance to the render object instance (for transforms, animation, etc.)
	bufferhandle glinstance_to_instance;

	// where are we getting our objects from
	const Render_Pass* parent_pass = nullptr;
};

// In theory render passes can be made once and cached if the object is static and do culling on the gpu
// Render lists are updated every frame though
// Render passes are  1 to 1 with render lists except gbuffer and shadow lists

// Render passes: (describes objects to be drawn in a render list with a material+geometry)
// Gbuffer objects
// Shadow casting objects
// Transparent objects
// Custom depth objects
// editor selected objects

// render lists: (contains glmultidrawelementsindirect commands, generated on cpu or on gpu)
// gbuffer pass 1 (all gbuffer objects get added to this)
//		gbuffer pass 1 gets culled to last HZB
// gbuffer pass 2 (objects that failed first HZB)
//		gets culled again to 2nd HZB
// shadow lists for N lights (cull per shadow caster, or render entire list per light)
// transparent list
// custom depth list
// editor selected list

struct RSunInternal
{
	Render_Sun sun;
	int unique_id = 0;
};

class TerrainInterfaceLocal;
class Render_Scene : public RenderScenePublic
{
public:
	Render_Scene();
	~Render_Scene();

	void init();

	// UGGGGGGGGH
	handle<Render_Object> register_obj() override {
		return { proxy_list.make_new() };
	}
	void update_obj(handle<Render_Object> handle, const Render_Object& proxy) override;
	void remove_obj(handle<Render_Object>& handle) override {
		if(handle.is_valid())
			proxy_list.free(handle.id);
		handle = { -1 };
	}
	const Render_Object& get(handle<Render_Object> handle) {
		return proxy_list.get(handle.id).proxy;
	}

	handle<Render_Light> register_light(const Render_Light& proxy) override { 
		handle<Render_Light> handle = { light_list.make_new() };
		update_light(handle, proxy);
		return handle;
	}
	void update_light(handle<Render_Light> handle, const Render_Light& proxy) override {
		auto& l = light_list.get(handle.id);
		l.light = proxy;
	}
	void remove_light(handle<Render_Light>& handle) override {
		if (!handle.is_valid())
			return;
		light_list.free(handle.id);
		handle = { -1 };
	}

	handle<Render_Decal> register_decal(const Render_Decal& decal) override {
		auto handle = decal_list.make_new();
		auto& internal = decal_list.get(handle);
		internal.decal = decal;
		internal.object = register_obj();
		return { -1 };

	}
	void update_decal(handle<Render_Decal> handle, const Render_Decal& decal) override {

	}
	void remove_decal(handle<Render_Decal>& handle) override {

	}
	handle<Render_Sun> register_sun(const Render_Sun& sun) override {
		handle<Render_Sun> id = { unique_id_counter++ };
		RSunInternal internal_sun;
		internal_sun.sun = sun;
		internal_sun.unique_id = id.id;
		suns.push_back(internal_sun);
		return id;
	}
	void update_sun(handle<Render_Sun> handle, const Render_Sun& sun) override {
		int i = 0;
		for (; i < suns.size(); i++) {
			if (suns[i].unique_id == handle.id)
				break;
		}
		if (i == suns.size()) {
			sys_print("??? update_sun couldn't find handle\n");
			return;
		}
		suns[i].sun = sun;
	}
	void remove_sun(handle<Render_Sun>& handle) override {
		if (!handle.is_valid())
			return;
		int i = 0;
		for (; i < suns.size(); i++) {
			if (suns[i].unique_id == handle.id)
				break;
		}
		if (i == suns.size()) {
			sys_print("??? remove_sun couldn't find handle\n");
			return;
		}
		suns.erase(suns.begin() + i);
		handle = { -1 };
	}
	handle<Render_Irradiance_Volume> register_irradiance_volume(const Render_Irradiance_Volume& vol) override {
		return { -1 };
	}
	void update_irradiance_volume(handle<Render_Irradiance_Volume> handle, const Render_Irradiance_Volume& sun) override {

	}
	void remove_irradiance_volume(handle<Render_Irradiance_Volume>& handle) override {

	}
	handle<Render_Reflection_Volume> register_reflection_volume(const Render_Reflection_Volume& vol) override {
		return { -1 };
	}
	void update_reflection_volume(handle<Render_Reflection_Volume> handle, const Render_Reflection_Volume& sun) override {

	}
	void remove_reflection_volume(handle<Render_Reflection_Volume>& handle)override {

	}
	handle<Render_Skylight> register_skylight(const Render_Skylight& vol) override {
		return { -1 };
	}
	void update_skylight(handle<Render_Skylight> handle, const Render_Skylight& sun)override {

	}
	void remove_skylight(handle<Render_Skylight>& handle) override {

	}

	virtual const Render_Object* get_read_only_object(handle<Render_Object> handle) override {
		if (!handle.is_valid()) return nullptr;
		return &proxy_list.get(handle.id).proxy;
	}

	void build_scene_data(bool is_for_editor);

	RSunInternal* get_main_directional_light();

	TerrainInterfacePublic* get_terrain_interface() override;

	std::unique_ptr<TerrainInterfaceLocal> terrain_interface;

	//std::unique_ptr<Render_Pass> gbuffer;
	//std::unique_ptr<Render_Lists> gbuffer1;				// main draw list, or 1st pass if using gpu culling
	//std::unique_ptr<Render_Lists> gbuffer2;				// 2nd pass for new unoccluded objects if using gpu culling
	//
	//std::unique_ptr<Render_Pass> transparent_objs;
	//std::unique_ptr<Render_Lists> transparents_ren_list;// draw in forward pass of transparents
	//
	//std::unique_ptr<Render_Pass> custom_depth;
	//std::unique_ptr<Render_Lists> custom_depth_list;	// draw to custom depth buffer
	//
	//std::unique_ptr<Render_Pass> editor_selection;
	//std::unique_ptr<Render_Lists> editor_sel_list;		// drawn to editor selection buffer
	//
	//std::unique_ptr<Render_Pass> shadow_casters;
	//std::unique_ptr<Render_Lists> global_shadow_list;	// unculled shadow casters


	Render_Pass gbuffer_pass;
	Render_Pass transparent_pass;
	Render_Pass shadow_pass;
	Render_Pass editor_sel_pass;

	Render_Lists gbuffer_rlist;
	Render_Lists transparent_rlist;
	Render_Lists csm_shadow_rlist;
	Render_Lists editor_sel_rlist;

	bufferhandle gpu_skinned_mats_buffer = 0;
	bufferhandle gpu_render_instance_buffer = 0;

	// use for stuff that isnt getting alloced much multiple times like suns,skylights
	uint32_t unique_id_counter = 0;

	Free_List<ROP_Internal> proxy_list;
	Free_List<RL_Internal> light_list;
	Free_List<RDecal_Internal> decal_list;
	// should just be one, but I let multiple ones exist too
	std::vector<RSunInternal> suns;
	std::vector<Render_Skylight> skylights;	// again should just be 1
	Free_List<Render_Reflection_Volume> reflection_volumes;
	Free_List<Render_Irradiance_Volume> irradiance_volumes;


	bufferhandle light_ssbo;
	bufferhandle light_grid_ssbo;
	bufferhandle indirect_to_light_ssbo;

	// list of IBL cubemaps and boxes
	// list of irradiance probe volumes and boxes

	uint32_t skybox = 0;
	std::vector<Render_Box_Cubemap> cubemaps;
	uint32_t cubemap_ssbo;
	uint32_t levelcubemapirradiance_array = 0;
	uint32_t levelcubemapspecular_array = 0;
	int levelcubemap_num = 0;

};