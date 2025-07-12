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

#include "GameEnginePublic.h"	// just for b_is_in_overlapped
#include "Framework/MeshBuilderImpl.h"

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
struct ROP_Internal;
class Render_Pass
{
public:
	Render_Pass(pass_type type);
	void make_batches(Render_Scene& scene);
	void merge_static_to_dynamic(bool* vis_array, int8_t* lod_array, Free_List<ROP_Internal>& proxy_list);
	void add_object(
		const Render_Object& proxy,
		handle<Render_Object> handle,
		const MaterialInstance* material,
		uint32_t camera_dist,
		int submesh,
		int lod,
		int layer, bool is_editor_mode);
	void add_static_object(
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

	void clear() { objects.clear();}
	void clear_static() { cached_static_objects.clear(); }
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
	glm::mat4 prev_transform{};
	int prev_bone_ofs = 0;
	glm::vec4 bounding_sphere_and_radius;
	bool is_static = true;
	bool has_init = false;
};
// RenderLight internal data
struct RL_Internal{
	Render_Light light;
	// stuff like shadowmap indicies etc.
	int shadow_array_handle = -1;
	bool updated_this_frame = false;
	glm::mat4 lightViewProj = glm::mat4(1.f);
};
struct RDecal_Internal{
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

enum class RenderObjectTypes
{
	Object,
	Sun,
	Light,
	Particle,
	Decal,
	Skylight,
	Reflection,
	Meshbuilder,
	Fog,
	Lightmap,
};

struct QueuedRenderObjectDelete
{
	int handle = -1;
	RenderObjectTypes type = RenderObjectTypes::Object;
};

struct MeshbuilderObj_Internal
{
	MeshBuilder_Object obj;
	MeshBuilderDD dd;
};
struct ParticleObj_Internal
{
	Particle_Object obj;
	MeshBuilderDD dd;
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
		ASSERT(!eng->get_is_in_overlapped_period());
		statics_meshes_are_dirty = true;
		handle<Render_Object> handle = { proxy_list.make_new() };
		return handle;
	}
	void update_obj(handle<Render_Object> handle, const Render_Object& proxy) override;
	void remove_obj(handle<Render_Object>& handle) override {
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Object);
			handle = { -1 };
			return;
		}
		statics_meshes_are_dirty = true;
		if (handle.is_valid()) {
			const bool was_static = proxy_list.get(handle.id).is_static;
			proxy_list.free(handle.id);
			statics_meshes_are_dirty |= was_static;
		}
		handle = { -1 };
	}
	const Render_Object& get(handle<Render_Object> handle) {
		return proxy_list.get(handle.id).proxy;
	}

	handle<Render_Light> register_light() override { 
		ASSERT(!eng->get_is_in_overlapped_period());
		handle<Render_Light> handle = { light_list.make_new() };
		//update_light(handle, proxy);
		return handle;
	}
	void update_light(handle<Render_Light> handle, const Render_Light& proxy) override;
	void remove_light(handle<Render_Light>& handle) override;

	handle<Render_Decal> register_decal() override {
		ASSERT(!eng->get_is_in_overlapped_period());
		auto handle = decal_list.make_new();
		//auto& internal = decal_list.get(handle);
		//internal.decal = decal;
		return { handle };

	}
	void update_decal(handle<Render_Decal> handle, const Render_Decal& decal) override {
		ASSERT(!eng->get_is_in_overlapped_period());
		if (!handle.is_valid()) 
			return;
		auto& i = decal_list.get(handle.id);
		i.decal = decal;
	}
	void remove_decal(handle<Render_Decal>& handle) override {
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Decal);
			handle = { -1 };
			return;
		}
		if (!handle.is_valid())
			return;
		decal_list.free(handle.id);
		handle = { -1 };
	}
	handle<Render_Sun> register_sun() override {
		ASSERT(!eng->get_is_in_overlapped_period());
		handle<Render_Sun> id = { int(unique_id_counter++) };
		RSunInternal internal_sun;
		//internal_sun.sun = sun;
		internal_sun.unique_id = id.id;
		suns.push_back(internal_sun);
		return id;
	}
	void update_sun(handle<Render_Sun> handle, const Render_Sun& sun) override {
		ASSERT(!eng->get_is_in_overlapped_period());
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
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Sun);
			handle = { -1 };
			return;
		}
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
	handle<Render_Reflection_Volume> register_reflection_volume() override {
		return { reflection_volumes.make_new() };
	}
	void update_reflection_volume(handle<Render_Reflection_Volume> handle, const Render_Reflection_Volume& sun) override {
		ASSERT(!eng->get_is_in_overlapped_period());
		if (!handle.is_valid())
			return;
		reflection_volumes.get(handle.id) = sun;
		reflection_volumes.get(handle.id).wants_update = true;
	}
	void remove_reflection_volume(handle<Render_Reflection_Volume>& handle)override {
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Skylight);
			handle = { -1 };
			return;
		}
		if (!handle.is_valid())
			return;
		auto vol = reflection_volumes.get(handle.id).generated_cube;
		if (vol) {
			vol->uninstall();
			delete vol;
		}
		reflection_volumes.free(handle.id);
		handle = { -1 };
	}
	handle<Render_Skylight> register_skylight() override {
		ASSERT(!eng->get_is_in_overlapped_period());
		handle<Render_Skylight> id = { int(unique_id_counter++) };
		RSkylight_Internal internal_skylight;
		//internal_skylight.skylight = skylight;
		internal_skylight.unique_id = id.id;
		skylights.push_back(internal_skylight);
		return id;
	}
	void update_skylight(handle<Render_Skylight> handle, const Render_Skylight& sky)override {
		ASSERT(!eng->get_is_in_overlapped_period());
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
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Skylight);
			handle = { -1 };
			return;
		}
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
	handle<RenderFog> register_fog() final {
		ASSERT(!eng->get_is_in_overlapped_period());
		if (has_fog) {
			sys_print(Warning, "only one fog allowed in a scene\n");
			return { -1 };
		}
		has_fog = true;
		//this->fog = fog;
		return { 0 };
	}
	void update_fog(handle<RenderFog> handle, const RenderFog& fog) final {
		ASSERT(!eng->get_is_in_overlapped_period());
		if (handle.is_valid()) {
			assert(has_fog);
			this->fog = fog;
		}
	}
	void remove_fog(handle<RenderFog>& handle) final {
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Fog);
		}
		else if (handle.is_valid()) {
			has_fog = false;
		}
		handle = { -1 };
	}

	virtual const Render_Object* get_read_only_object(handle<Render_Object> handle) override {
		if (!handle.is_valid()) return nullptr;
		return &proxy_list.get(handle.id).proxy;
	}

	virtual handle<MeshBuilder_Object> register_meshbuilder() final {
		ASSERT(!eng->get_is_in_overlapped_period());
		int handle = meshbuilder_objs.make_new();
		//meshbuilder_objs.get(handle).obj = mbobj;
		return { handle };
	}
	virtual void update_meshbuilder(handle<MeshBuilder_Object> handle, const MeshBuilder_Object& mbobj) final {
		ASSERT(!eng->get_is_in_overlapped_period());
		assert(handle.is_valid());
		meshbuilder_objs.get(handle.id).obj = mbobj;
	}
	virtual void remove_meshbuilder(handle<MeshBuilder_Object>& handle) final {
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Meshbuilder);
		}
		else if (handle.is_valid()) {
			meshbuilder_objs.free(handle.id);
		}
		handle = { -1 };
	}

	Free_List<ParticleObj_Internal> particle_objs;
	virtual handle<Particle_Object> register_particle_obj() final {
		ASSERT(!eng->get_is_in_overlapped_period());
		int handle = particle_objs.make_new();
		//particle_objs.get(handle).obj = mbobj;
		return { handle };
	}
	virtual void update_particle_obj(handle<Particle_Object> handle, const Particle_Object& mbobj) final {
		ASSERT(!eng->get_is_in_overlapped_period());
		assert(handle.is_valid());
		particle_objs.get(handle.id).obj = mbobj;
	}
	virtual void remove_particle_obj(handle<Particle_Object>& handle) final {
		if (eng->get_is_in_overlapped_period()){
			add_to_queued_deletes(handle.id, RenderObjectTypes::Particle);
		}
		else if (handle.is_valid()) {
			particle_objs.free(handle.id);
		}
		handle = { -1 };
	}
	handle<Lightmap_Object> register_lightmap() final {
		if (has_lightmap) {
			sys_print(Warning, "RenderScene::register_lightmap: already has lightmap, this handle is now empty\n");
			return { -1 };
		}
		has_lightmap = true;
		return { 0 };
	}
	void update_lightmap(handle<Lightmap_Object> lightmap, Lightmap_Object& obj) final {
		assert(!eng->get_is_in_overlapped_period());
		if (!lightmap.is_valid()) {
			sys_print(Warning, "RenderScene::update_lightmap: invalid lightmap handle\n");
			return;
		}
		assert(lightmap.id == 0);
		assert(has_lightmap);
		lightmapObj = std::move(obj);
	}
	void remove_lightmap(handle<Lightmap_Object> handle) final {
		if (!handle.is_valid()) {
			return;
		}
		if (eng->get_is_in_overlapped_period()) {
			add_to_queued_deletes(handle.id, RenderObjectTypes::Lightmap);
		}
		else if (handle.is_valid()) {
			has_lightmap = false;
			lightmapObj = Lightmap_Object();
		}
		handle = { -1 };
	}

	void add_to_queued_deletes(int id, RenderObjectTypes type)
	{
		queued_deletes.push_back({ id,type });
	}
	void execute_deferred_deletes() {
		ASSERT(!eng->get_is_in_overlapped_period());
		for (auto& qd : queued_deletes) {
			switch (qd.type)
			{
			case RenderObjectTypes::Decal: {
				handle<Render_Decal> h{ qd.handle };
				remove_decal(h);
			}break;
			case RenderObjectTypes::Object: {
				handle<Render_Object> h{ qd.handle };
				remove_obj(h);
			}break;
			case RenderObjectTypes::Light: {
				handle<Render_Light> h{ qd.handle };
				remove_light(h);
			}break;
			case RenderObjectTypes::Skylight: {
				handle<Render_Skylight> h{ qd.handle };
				remove_skylight(h);
			}break;
			case RenderObjectTypes::Meshbuilder: {
				handle<MeshBuilder_Object> h{ qd.handle };
				remove_meshbuilder(h);
			}break;
			case RenderObjectTypes::Particle: {
				handle<Particle_Object> h{ qd.handle };
				remove_particle_obj(h);
			}break;
			case RenderObjectTypes::Sun: {
				handle<Render_Sun> h{ qd.handle };
				remove_sun(h);
			}break;
			case RenderObjectTypes::Reflection: {
				handle<Render_Reflection_Volume> h{ qd.handle };
				remove_reflection_volume(h);
			}break;
			case RenderObjectTypes::Fog: {
				handle<RenderFog> h{ qd.handle };
				remove_fog(h);
			}break;
			case RenderObjectTypes::Lightmap: {
				handle<Lightmap_Object> h{ qd.handle };
				remove_lightmap(h);
			}break;
			default:
				ASSERT(!"no type defined for queued delete render");
			}
		}
		queued_deletes.clear();
	}

	void evaluate_lighting_at_position(const glm::vec3& pos, glm::vec3* ambientCubeOut /* expects size 6 vector*/) const {
		auto& vols = reflection_volumes.objects;
		if (has_lightmap) {
			for (auto& [_, vol] : vols) {
				if (vol.wants_update || vol.probe_ofs == -1)
					continue;
				Bounds b(vol.boxmin, vol.boxmax);
				if (b.inside(pos, 0)) {
					auto& probes = lightmapObj.staticAmbientCubeProbes;
					for (int i = 0; i < 6; i++)
						ambientCubeOut[i] = probes.at(vol.probe_ofs * 6 + i);
					return;	// early out
				}
			}
		}
		// hasnt found yet
		if (!skylights.empty()&&!skylights.at(0).skylight.wants_update) {
			for (int i = 0; i < 6; i++)
				ambientCubeOut[i] = skylights.at(0).ambientCube[i];
			return;
		}
		// set to constant
		for (int i = 0; i < 6; i++)
			ambientCubeOut[i] = glm::vec3(0.1);
	}

	void build_scene_data(bool skybox_only, bool is_for_editor);
	void refresh_static_mesh_data(bool is_for_editor);
	RSunInternal* get_main_directional_light();
	TerrainInterfacePublic* get_terrain_interface() override;
	std::unique_ptr<TerrainInterfaceLocal> terrain_interface;

	bool statics_meshes_are_dirty = false;
	bool static_cache_built_for_editor = false;
	bool static_cache_built_for_debug = false;

	Render_Pass gbuffer_pass;
	Render_Pass transparent_pass;
	Render_Pass editor_sel_pass;
	Render_Pass shadow_pass;	// all shadow casting objects
	Render_Lists gbuffer_rlist;
	Render_Lists transparent_rlist;
	std::vector<Render_Lists> cascades_rlists;	// lists specific to each cascade, culled
	Render_Lists editor_sel_rlist;
	Render_Lists spotLightShadowList;


	int get_front_bone_buffer_offset() const {
		return gpu_skinned_mats_using_front_buffer ? 0 : gpu_skinned_mats_buffer_size / 2;
	}
	int get_back_bone_buffer_offset() const {
		return gpu_skinned_mats_using_front_buffer ? gpu_skinned_mats_buffer_size / 2 : 0;
	}
	void flip_bone_buffers() {
		gpu_skinned_mats_using_front_buffer = !gpu_skinned_mats_using_front_buffer;
	}

	int gpu_skinned_mats_buffer_size = 0;	// in matricies (64 bytes)
	bool gpu_skinned_mats_using_front_buffer = true;
	bufferhandle gpu_skinned_mats_buffer = 0;
	bufferhandle gpu_render_instance_buffer = 0;

	bool has_lightmap = false;
	Lightmap_Object lightmapObj;


	Free_List<ROP_Internal> proxy_list;
	Free_List<MeshbuilderObj_Internal> meshbuilder_objs;
	Free_List<RL_Internal> light_list;
	Free_List<RDecal_Internal> decal_list;
	// should just be one, but I let multiple ones exist too
	std::vector<RSunInternal> suns;
	std::vector<RSkylight_Internal> skylights;	// again should just be 1
	Free_List<Render_Reflection_Volume> reflection_volumes;

	// objects can be deleted mid frame, so queue them
	std::vector<QueuedRenderObjectDelete> queued_deletes;

	bool has_fog = false;
	RenderFog fog;
	
	// use for stuff that isnt getting alloced much multiple times like suns,skylights
	int unique_id_counter = 0;
};