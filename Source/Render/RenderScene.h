#pragma once
#include "Render/DrawPublic.h"

// All the renderable types
#include "Render/RenderObj.h"
#include "Render/Render_Decal.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "Render/Render_Volumes.h"
#include "Render/DrawTypedefs.h"
#include "Render/MaterialLocal.h"
#include "Render/RenderFog.h"

#include "Framework/FreeList.h"

#include "../Shaders/SharedGpuTypes.txt"

#include <cstdint>


class MaterialInstance;
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
	int first = 0;	// indexes into pass.sorted_list
	int count = 0;

	int shader_index = 0;	// indexes into shader_list[]
	const MaterialInstance* material = nullptr;
};

// represents multiple Mesh_Batch calls packaged into one glMultidrawIndirect()
struct Multidraw_Batch
{
	int first = 0;
	int count = 0;
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
	const MaterialInstance* material = nullptr;
	handle<Render_Object> render_obj{};	// entity instance
	short submesh_index = 0;		// what submesh am i
	short lod_index = 0;
	int hl_obj_index{};
	int batch_idx = 0;
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
		const MaterialInstance* material,
		uint32_t camera_dist,
		int submesh,
		int lod,
		int layer, bool is_editor_mode);

	draw_call_key create_sort_key_from_obj(
		const Render_Object& proxy,
		const MaterialInstance* material,
		uint32_t camera_dist,
		int submesh,
		int layer, bool is_editor_mode);

	void clear() {
		objects.clear();
	}
	void clear_static() {
		cached_static_objects.clear();
	}

	const pass_type type{};					// modifies batching+sorting logic


	std::vector<Pass_Object> objects;		// geometry + material id + object id
	std::vector<Pass_Object> cached_static_objects;	// copied into objects

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

struct RDecal_Internal
{
	Render_Decal decal;
};


struct Render_Lists
{
	void init(uint32_t drawidsz, uint32_t instbufsz);

	void build_from(Render_Pass& src,
		Free_List<ROP_Internal>& proxy_list);


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


struct RSkylight_Internal
{
	Render_Skylight skylight;
	glm::vec3 ambientCube[6];
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
		handle<Render_Object> handle = { proxy_list.make_new() };
		return handle;
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
		return { handle };

	}
	void update_decal(handle<Render_Decal> handle, const Render_Decal& decal) override {
		if (!handle.is_valid()) 
			return;
		auto& i = decal_list.get(handle.id);
		i.decal = decal;
	}
	void remove_decal(handle<Render_Decal>& handle) override {
		if (!handle.is_valid())
			return;
		decal_list.free(handle.id);
		handle = { -1 };
	}
	handle<Render_Sun> register_sun(const Render_Sun& sun) override {
		handle<Render_Sun> id = { int(unique_id_counter++) };
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
			sys_print(Warning, "update_sun couldn't find handle\n");
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
			sys_print(Warning, "remove_sun couldn't find handle\n");
			return;
		}
		suns.erase(suns.begin() + i);
		handle = { -1 };
	}
	handle<Render_Reflection_Volume> register_reflection_volume(const Render_Reflection_Volume& vol) override {
		return { -1 };
	}
	void update_reflection_volume(handle<Render_Reflection_Volume> handle, const Render_Reflection_Volume& sun) override {

	}
	void remove_reflection_volume(handle<Render_Reflection_Volume>& handle)override {

	}
	handle<Render_Skylight> register_skylight(const Render_Skylight& skylight) override {
		handle<Render_Skylight> id = { int(unique_id_counter++) };
		RSkylight_Internal internal_skylight;
		internal_skylight.skylight = skylight;
		internal_skylight.unique_id = id.id;
		skylights.push_back(internal_skylight);
		return id;
	}
	void update_skylight(handle<Render_Skylight> handle, const Render_Skylight& sky)override {
		int i = 0;
		for (; i < skylights.size(); i++) {
			if (skylights[i].unique_id == handle.id)
				break;
		}
		if (i == skylights.size()) {
			sys_print(Warning, "update_skylight couldn't find handle\n");
			return;
		}
		skylights[i].skylight = sky;
	}
	void remove_skylight(handle<Render_Skylight>& handle) override {
		if (!handle.is_valid())
			return;
		int i = 0;
		for (; i < skylights.size(); i++) {
			if (skylights[i].unique_id == handle.id)
				break;
		}
		if (i == skylights.size()) {
			sys_print(Warning, "remove_skylight couldn't find handle\n");
			return;
		}
		skylights.erase(skylights.begin() + i);
		handle = { -1 };
	}
	handle<RenderFog> register_fog(const RenderFog& fog) {
		if (has_fog) {
			sys_print(Warning, "only one fog allowed in a scene\n");
			return { -1 };
		}
		has_fog = true;
		this->fog = fog;
		return { 0 };
	}
	void update_fog(handle<RenderFog> handle, const RenderFog& fog) {
		if (handle.is_valid()) {
			assert(has_fog);
			this->fog = fog;
		}
	}
	void remove_fog(handle<RenderFog>& handle) {
		if (handle.is_valid()) {
			has_fog = false;
			handle = { -1 };
		}
	}

	virtual const Render_Object* get_read_only_object(handle<Render_Object> handle) override {
		if (!handle.is_valid()) return nullptr;
		return &proxy_list.get(handle.id).proxy;
	}


	Free_List<MeshBuilder_Object> meshbuilder_objs;
	virtual handle<MeshBuilder_Object> register_meshbuilder(const MeshBuilder_Object& mbobj) {
		int handle = meshbuilder_objs.make_new();
		meshbuilder_objs.get(handle) = mbobj;
		return { handle };
	}
	virtual void update_meshbuilder(handle<MeshBuilder_Object> handle, const MeshBuilder_Object& mbobj) {
		assert(handle.is_valid());
		meshbuilder_objs.get(handle.id) = mbobj;
	}
	virtual void remove_meshbuilder(handle<MeshBuilder_Object>& handle) {
		if (handle.is_valid()) {
			meshbuilder_objs.free(handle.id);
			handle = { -1 };
		}
	}

	Free_List<Particle_Object> particle_objs;
	virtual handle<Particle_Object> register_particle_obj(const Particle_Object& mbobj) {
		int handle = particle_objs.make_new();
		particle_objs.get(handle) = mbobj;
		return { handle };
	}
	virtual void update_particle_obj(handle<Particle_Object> handle, const Particle_Object& mbobj) {
		assert(handle.is_valid());
		particle_objs.get(handle.id) = mbobj;
	}
	virtual void remove_particle_obj(handle<Particle_Object>& handle) {
		if (handle.is_valid()) {
			particle_objs.free(handle.id);
			handle = { -1 };
		}
	}


	void build_scene_data(bool skybox_only, bool is_for_editor);

	RSunInternal* get_main_directional_light();

	TerrainInterfacePublic* get_terrain_interface() override;

	std::unique_ptr<TerrainInterfaceLocal> terrain_interface;


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
	std::vector<glm::vec4> proxy_list_bounding_spheres;	// SOA bounding spheres for culling

	Free_List<RL_Internal> light_list;
	Free_List<RDecal_Internal> decal_list;
	// should just be one, but I let multiple ones exist too
	std::vector<RSunInternal> suns;
	std::vector<RSkylight_Internal> skylights;	// again should just be 1
	Free_List<Render_Reflection_Volume> reflection_volumes;

	bool has_fog = false;
	RenderFog fog;


	bufferhandle light_ssbo;
	bufferhandle light_grid_ssbo;
	bufferhandle indirect_to_light_ssbo;
};