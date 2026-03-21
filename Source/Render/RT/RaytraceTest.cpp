#include "RaytraceTest.h"
#include "Render/DrawLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"

#include "Framework/MathLib.h"

const int MAX_RAYS = 256;
const int ddgiIRRADTILE = 8;
const int ddgiDEPTHTILE = 16;

using std::vector;


DdgiTesting::DdgiTesting()
{
	raytrace_test = draw.get_prog_man().create_raster("fullscreenquad.txt", "rtF.txt");

	gather_shader = draw.get_prog_man().create_compute("gather_C.txt");
		shade_fs = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeF.txt");
		shade_fs_halfres = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeF.txt","HALFRES_DDGI");

		shade_debug_fs = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeDebugF.txt");

	//	while (1) {
			trace_shader = draw.get_prog_man().create_compute("trace_C.txt");
	//	};
	debug_probes = draw.get_prog_man().create_raster("MeshSimpleV.txt", "MeshDebugProbeF.txt");

	get_best_cubemap_shader = draw.get_prog_man().create_compute("get_best_cubemap_C.txt");

	relocate_shader = draw.get_prog_man().create_compute("trace_C.txt", "RELOCATE");

	lum_calc = draw.get_prog_man().create_compute("probeLumCalc_C.txt");

	avg_probe_calc = draw.get_prog_man().create_compute("avgProbeCalc_C.txt");

	bilateral_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "bilateral_upsample_ddgi.txt");
	temporal_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "temporal_upsample_ddgi.txt");
	apply_halfres_accum_to_scene = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgi_apply_upsampled.txt");

	ddgi_probe_relocation_offsets = IGraphicsDevice::inst->create_buffer({});
	ddgi_globals = IGraphicsDevice::inst->create_buffer({});
	ddgi_volumes = IGraphicsDevice::inst->create_buffer({});

	ddgi_probe_avg_value = IGraphicsDevice::inst->create_buffer({});


	Texture::install_system("_ddgi");
	Texture::install_system("_ddgi_d");

}
//FIXME
#include "Game/Components/LightComponents.h"
#include "Level.h"
#include "Game/Entity.h"
struct VolumesAndNumProbes {
	vector<DdgiVolumeGpu> volumes;
	int num_probes;

	vector<glm::vec4> relocate_data;
};
extern float relocate_normal_dist;
#include <algorithm>
VolumesAndNumProbes find_volumes() {
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	vector<DdgiVolumeGpu> volumes;
	int probes_summation = 0;
	vector<GiVolumeComponent*> comps;
	for (auto o : objs) {
		if (auto givol = o->cast_to<GiVolumeComponent>()) {
			comps.push_back(givol);
		}
	}
	std::sort(comps.begin(), comps.end(), [](GiVolumeComponent* a, GiVolumeComponent* b) {
		return a->priority > b->priority;
		});
	vector<vec4> relocate_volume_params;
	for (auto givol : comps) {
		glm::mat4 transform = givol->get_ws_transform();
		glm::vec3 min1 = transform * glm::vec4(-0.5, -0.5, -0.5, 1);
		glm::vec3 max1 = transform * glm::vec4(0.5, 0.5, 0.5, 1);
		glm::vec3 min = glm::min(min1, max1);
		glm::vec3 max = glm::max(min1, max1);


		DdgiVolumeGpu volume{};
		volume.density = glm::vec4(givol->xz_density, givol->y_density, givol->xz_density, 0);
		volume.origin_priority = glm::vec4(min, givol->priority);
		glm::vec3 size = max - min;
		glm::ivec3 probe_size = (glm::ivec3)glm::round(size / glm::vec3(volume.density)) + glm::ivec3(1);
		volume.size_offset = glm::ivec4(probe_size, probes_summation);
		probes_summation += volume.get_num_probes_total();
		volumes.push_back(volume);
		if (givol->override_relocate_dist) {
			relocate_volume_params.push_back(vec4(givol->relocate_max_dist, givol->relocate_normal_push, 0, 0));
		}
		else {
			relocate_volume_params.push_back(vec4(draw.ddgi->max_relocate_dist, relocate_normal_dist, 0, 0));
		}
	}
	return { volumes,probes_summation,relocate_volume_params };
}

void raytrace_the_world() {
	auto [volumes, num_probes, relocate_params] = find_volumes();
	// allocate probe irradiance and depth
	auto self = draw.ddgi.get();
	self->probe_irradiance->release();
	self->probe_depth->release();
	ASSERT(relocate_params.size() == volumes.size());

	const int height_probe_space = 128;
	const int width_probe_space = (int)glm::ceil(num_probes / float(height_probe_space));

	DdgiGlobals globals{};
	globals.atlas_x = width_probe_space;
	globals.atlas_y = height_probe_space;
	globals.num_volumes = volumes.size();
	//globals.relocate_normal_dist = relocate_normal_dist;

	// rt algorithm:
	/*
	
	build the rt structure:
		


	for each volume:
		relocate_probes()

	for each bounce
		for each volume
			trace-rays()
		gather()


	rendering {

		volume:sample(pos)
			grid_xyz = get_grid(pos)
			for corner in corners:
				relocate_offset = get(corner)

		apply_gi
			worldpos,normal = get_gbuffer()
			for each volume:
				if worldpos in volume:
					indirect = volume.sample()
			if no volumes:
				sample_sky()
	}

	*/

}



DdgiTesting::~DdgiTesting()
{

}
inline Bounds get_tri_bounds(vec3 v1, vec3 v2, vec3 v3)
{
	Bounds b(v1);
	b = bounds_union(b, v2);
	b = bounds_union(b, v3);
	return b;
}
ConfigVar vert_limit("vert_limit", "9999999", CVAR_INTEGER | CVAR_UNBOUNDED, "");
Color32 get_color_of_material_for_export(const MaterialInstance* m);
#include "Assets/AssetDatabase.h"
void set_shit_fuck();


void DdgiTesting::compute_avg_probe_value()
{
	const int num_probes = temp_probe_relocate_thing.size();
	ddgi_probe_avg_value->upload(nullptr, num_probes * sizeof(float) * 3);

	if (!probe_depth || !probe_irradiance) return;
	auto& device = draw.get_device();
	device.bind_texture(2, probe_irradiance->get_internal_handle());
	device.bind_texture(3, probe_depth->get_internal_handle());

	device.set_shader(avg_probe_calc);
	set_shit_fuck();

	const int groups = glm::ceil(num_probes / 64.f);
	printf("compute_avg_probe_value %d\n", groups);


	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, ddgi_probe_avg_value->get_internal_handle());
	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
void DdgiTesting::create_textures_raybuffer(int probe_width, int probe_height) {

	
	if (this->probe_depth)
		this->probe_depth->release();
	if (this->probe_irradiance)
		this->probe_irradiance->release();


	CreateTextureArgs targs;
	const int tiles_wide = probe_width;
	const int tiles_height = probe_height;
	targs.width = tiles_wide * ddgiIRRADTILE;
	targs.height = tiles_height * ddgiIRRADTILE;

	targs.type = GraphicsTextureType::t2D;
	targs.num_mip_maps = 1;
	targs.format = GraphicsTextureFormat::r11f_g11f_b10f;
	targs.sampler_type = GraphicsSamplerType::LinearNoMipmaps;
	probe_irradiance = IGraphicsDevice::inst->create_texture(targs);
	probe_irradiance->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);

	auto handle = Texture::load("_ddgi");
	handle->update_specs_ptr(this->probe_irradiance);

	targs.width = tiles_wide * ddgiDEPTHTILE;
	targs.height = tiles_height * ddgiDEPTHTILE;
	targs.format = GraphicsTextureFormat::rg16f;
	probe_depth = IGraphicsDevice::inst->create_texture(targs);
	probe_depth->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);


	handle = Texture::load("_ddgi_d");
	handle->update_specs_ptr(this->probe_depth);
}



extern glm::vec3 get_emissive_of_mat_for_export(const MaterialInstance* m);

void DdgiTesting::build_world()
{
	auto& objs = draw.scene.proxy_list;

	std::vector<Bounds> bounds;
	//std::vector<int> all_indicies;
	std::vector<glm::vec4> all_verticies;

	
	int counter = 0;
	for (auto& _o : objs.objects) {
		auto& o = _o.type_.proxy;
		if (!o.model)
			continue;
		if (o.is_skybox)
			continue;
		if (o.ignore_in_baking)
			continue;
		if (o.model->get_num_lods() == 0)
			continue;
		counter += 1;

		auto matoverride = o.mat_override;

		// upload transformed verts, indicies, add bounds

		const glm::mat4 transform = o.transform;
		auto rmd = o.model->get_raw_mesh_data();

		if (rmd->get_num_verticies(0) > vert_limit.get_integer())
			continue;

		// use lod1? idk want less verts but not shitty verts
		const int lod_to_use = glm::min(1, (int)o.model->get_num_lods() - 1);
		auto& lod = o.model->get_lod(lod_to_use);

		// for all parts
		for (int parti = lod.part_ofs; parti < lod.part_ofs+lod.part_count; parti++) {
			//const int index_start = all_indicies.size();
			const int vertex_start = all_verticies.size();

			const auto& part = o.model->get_part(parti);
			auto material_inst = o.model->get_material_for_part(part);
			if (matoverride)
				material_inst = matoverride;

			if (!material_inst) continue;
			auto material = material_inst->impl.get();
			if (!material||!material->get_master_impl())
				continue;
			if (material->get_master_impl()->light_mode == LightingMode::Unlit)
				continue;
			

			const int material_offset = material->gpu_buffer_offset == -1 ? 0 : material->get_material_index_from_buffer_ofs();

			const int part_index_start = part.element_offset / 2;
			const int part_index_count = part.element_count;
			const int base_vertex = part.base_vertex;
			const int num_verts = part.vertex_count;

			auto get_vertex = [&](int index) ->glm::vec3 {
				return transform * glm::vec4(rmd->get_vertex_at_index(base_vertex+index).pos, 1.0);
			};

			//for (int i = 0; i < num_verts; i++) {
			//	all_verticies.push_back(glm::vec4(get_vertex(i), material_offset));
			//}
			for (int i = 0; i < part_index_count; i+=3) {
				const int i0 = rmd->get_index_at_index(part_index_start+i);
				const int i1 = rmd->get_index_at_index(part_index_start+i+1);
				const int i2 = rmd->get_index_at_index(part_index_start+i+2);

				//all_indicies.push_back(i0+vertex_start);
				//all_indicies.push_back(i1+vertex_start);
				//all_indicies.push_back(i2+vertex_start);
				all_verticies.push_back(glm::vec4(get_vertex(i0),material_offset));
				all_verticies.push_back(glm::vec4(get_vertex(i1), material_offset));
				all_verticies.push_back(glm::vec4(get_vertex(i2), material_offset));

				const Bounds tri_bounds = get_tri_bounds(get_vertex(i0), get_vertex(i1), get_vertex(i2));
				bounds.push_back(tri_bounds);
			}
		}
	}
	const double start = GetTime();
	BVH as = BVH::build(bounds, 4, PartitionStrategy::BVH_SAH);
	printf("rt build bvh time: %f\n", float(GetTime() - start));

	// Build for GPU
	std::vector<GPUBVHNode> nodes;
	for (int i = 0; i < as.nodes.size(); i++) {
		GPUBVHNode n;
		n.min = vec4(as.nodes[i].aabb.bmin, 1);
		n.max = vec4(as.nodes[i].aabb.bmax, 1);
		n.count = as.nodes[i].count;
		n.left_node = as.nodes[i].left_node;
		nodes.push_back(n);
	}

	std::vector<glm::vec4> materialsdata;
	auto& allmats = matman.get_material_table()->get_all_mat_array();
	materialsdata.resize(MAX_MATERIALS);
	for (int i = 0; i < allmats.size(); i++) {
		if(!allmats.at(i))
			continue;
		auto m = (MaterialImpl*)allmats.at(i)->impl.get();
		if (m->gpu_buffer_offset == MaterialImpl::INVALID_MAPPING)
			continue;
		const int INDEX = m->get_material_index_from_buffer_ofs();

		auto vec = color32_to_vec4(get_color_of_material_for_export(allmats.at(i)));
		auto linear = glm::pow(vec, glm::vec4(2.2));
		linear.w = 1;

		auto emissive = get_emissive_of_mat_for_export(allmats.at(i));
		if (glm::dot(emissive, glm::vec3(1)) > 0.1) {
			linear = glm::vec4(emissive, 0);
		}

		materialsdata.at(INDEX) = linear;
	}

	if (verts)
		verts->release();
	if (indicies)
		indicies->release();
	if (references)
		references->release();
	if (this->nodes)
		this->nodes->release();
	if (this->materials)
		this->materials->release();


	CreateBufferArgs args;
	args.flags = GraphicsBufferUseFlags::BUFFER_USE_AS_STORAGE_READ;
	args.size = all_verticies.size() * sizeof(glm::vec4);
	verts = IGraphicsDevice::inst->create_buffer(args);
	verts->upload(all_verticies.data(), args.size);

	//args.size = sizeof(int) * all_indicies.size();
	//indicies = IGraphicsDevice::inst->create_buffer(args);
	//indicies->upload(all_indicies.data(), args.size);


	args.size = sizeof(int) * as.indicies.size();
	references = IGraphicsDevice::inst->create_buffer(args);
	references->upload(as.indicies.data(), args.size);

	args.size = nodes.size() * sizeof(GPUBVHNode);
	this->nodes = IGraphicsDevice::inst->create_buffer(args);
	this->nodes->upload(nodes.data(), args.size);
	
	args.size = materialsdata.size() * sizeof(glm::vec4);
	this->materials = IGraphicsDevice::inst->create_buffer(args);
	this->materials->upload(materialsdata.data(), args.size);

}
#include "imgui.h"
static float irrad_mult = 1.0;
static float normal_bias = 0.2;
static float view_bias = 0.3;
static int bounces = 4;
static bool include_cubemaps = true;
static float relocate_normal_dist = 0.2;
static int lum_adjust_mode = 4;
static float depth_sigma = 50.0;
static float normal_sigma = 1.0;
static bool wants_half_res = false;
static bool do_flush_after = true;
static bool skip_gather = false;
static vec2 ssr_lum_range = vec2(0, 0.1);
void ddgi_debugmenu() {
	auto self = draw.ddgi.get();
	ImGui::InputInt3("select", &self->selected_probe.x);
	
	ImGui::DragFloat("irradmul", &irrad_mult, 0.01);

	if (ImGui::Button("refresh"))
		self->execute();
	ImGui::InputFloat("normal bias", &normal_bias);
	ImGui::InputFloat("view bias", &view_bias);
	ImGui::InputInt("bounces", &bounces);
	ImGui::Checkbox("reflections", &include_cubemaps);
	ImGui::InputFloat("relocate_normal_dist", &relocate_normal_dist);
	ImGui::SliderInt("lum_adjust", &lum_adjust_mode, 0, 7);

	ImGui::InputFloat("max_relocate_dist", &self->max_relocate_dist);
	ImGui::InputFloat("indirect_boost", &self->indirect_boost);

	ImGui::InputFloat("depth_sigma", &depth_sigma);
	ImGui::InputFloat("normal_sigma", &normal_sigma);

	ImGui::Checkbox("wants_half_res", &wants_half_res);
	ImGui::Checkbox("do_flush", &do_flush_after);
	ImGui::Checkbox("skip_gather", &skip_gather);
	ImGui::InputFloat("ssr_lum_min", &ssr_lum_range.x);
	ImGui::InputFloat("ssr_lum_max", &ssr_lum_range.y);
}
ADD_TO_DEBUG_MENU(ddgi_debugmenu);
void set_shit_fuck() {
	
	auto self = draw.ddgi.get();

	glBindBufferBase(GL_UNIFORM_BUFFER, 8, self->ddgi_globals->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, self->ddgi_volumes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, self->ddgi_probe_relocation_offsets->get_internal_handle());

}


ConfigVar r_rt_skiprebuild("r.rt_skiprebuild", "0", CVAR_BOOL, "");
void DdgiTesting::execute()
{
	if(!verts||!r_rt_skiprebuild.get_bool())
		build_world();
	double start = GetTime();

	// run trace
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verts->get_internal_handle());
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indicies->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());
	
	
	auto [volumes, total_num_probes, relocate_params] = find_volumes();
	if (volumes.size() == 0) throw 1;
	ASSERT(relocate_params.size() == volumes.size());
	
	this->myvolumes = volumes;
	// allocate probe irradiance and depth

	const int height_probe_space = 128;
	const int width_probe_space = (int)glm::ceil(total_num_probes / float(height_probe_space));

	DdgiGlobals globals{};
	globals.atlas_x = width_probe_space;
	globals.atlas_y = height_probe_space;
	globals.num_volumes = volumes.size();
	globals.max_relocate_dist = max_relocate_dist;
	globals.indirect_boost = indirect_boost;
	globals.relocate_normal_dist = relocate_normal_dist;
	ddgi_globals->upload(&globals, sizeof(globals));
	ddgi_volumes->upload(volumes.data(), volumes.size() * sizeof(DdgiVolumeGpu));
	theglobals = globals;

	ddgi_probe_relocation_offsets->upload(nullptr, sizeof(glm::vec4) * total_num_probes);


	CreateBufferArgs args{};
	args.size = total_num_probes * MAX_RAYS * sizeof(RayBufferStruct);
	IGraphicsBuffer* ray_buffer = IGraphicsDevice::inst->create_buffer(args);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ray_buffer->get_internal_handle());



	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, draw.buf.lighting_uniforms->get_internal_handle());

	create_textures_raybuffer(width_probe_space, height_probe_space);


	auto& device = draw.get_device();


	IGraphicsBuffer* invalid_count_buf{};
	invalid_count_buf = IGraphicsDevice::inst->create_buffer({});
	glm::ivec4 counter_num = {};
	invalid_count_buf->upload(&counter_num, sizeof(glm::ivec4));
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, invalid_count_buf->get_internal_handle());


	// relocation
	Random r(13);
	{
		device.set_shader(relocate_shader);

		// bruh moment
		IGraphicsBuffer* relocate_param_buf = IGraphicsDevice::inst->create_buffer({});
		relocate_param_buf->upload(relocate_params.data(), relocate_params.size() * sizeof(vec4));
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, relocate_param_buf->get_internal_handle());

		set_shit_fuck();
		device.shader().set_float("ray_sample_randomness", r.RandF(0, TWOPI));
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, ddgi_probe_relocation_offsets->get_internal_handle());
		const int total_probes = total_num_probes;
		const int groups = glm::ceil(total_probes / 64.f);
		glDispatchCompute(groups, 1, 1);

		{
			temp_probe_relocate_thing.resize(total_num_probes);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ddgi_probe_relocation_offsets->get_internal_handle());
			glm::vec4* ptr = (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			for (int i = 0; i < total_num_probes; i++)
				temp_probe_relocate_thing.at(i) = ptr[i];
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		}

		relocate_param_buf->release();
	}
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	for (int i = 0; i < bounces; i++) {
		device.bind_texture(5, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());

		device.bind_texture(2, probe_irradiance->get_internal_handle());
		device.bind_texture(3, probe_depth->get_internal_handle());


		device.set_shader(trace_shader);
		// dont know if this makes sense. want to start averaging for run i=1,2,3.
		// at i=0, there will be no indirect so i dont think accumulating there will be totally right
		device.shader().set_bool("do_irrad_calcs", i!=0);
		set_shit_fuck();
		device.shader().set_float("ray_sample_randomness", r.RandF(0,TWOPI));
		device.shader().set_int("num_lights", draw.scene.light_list.objects.size());


		if (draw.scene.suns.size() > 0) {
			auto& sun = draw.scene.suns.at(0).sun;
			device.shader().set_vec4("light_sun_dir", vec4(sun.direction, 1));
			device.shader().set_vec4("light_sun_color", vec4(sun.color, 0));
		}
		else {
			device.shader().set_vec4("light_sun_dir", vec4(0,0,0, -1));
		}

		const int total_probes = total_num_probes;
		const int groups = total_probes;// glm::ceil(total_probes / 64.f);

		printf("trace %d\n", i);
		glDispatchCompute(groups, 1, 1);

		if (!skip_gather) {
			// then run gather
			device.set_shader(gather_shader);
			set_shit_fuck();
			device.shader().set_int("num_runs_so_far", glm::max(0, i - 1));



			glBindImageTexture(0, probe_irradiance->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);

			glBindImageTexture(1, probe_depth->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

			glCheckError();
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		printf("gather %d\n", i);
		const int NUM_LOCAL_INNVOC = 232;

		const int gather_groups = total_probes;	// one global innvoc per probe
		const int gather_groups_prev = glm::ceil(total_probes / 64.f);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, ddgi_probe_relocation_offsets->get_internal_handle());	// bind for writing...

		glDispatchCompute(gather_groups, 1, 1);
		glCheckError();

		if (i == 0) {
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, invalid_count_buf->get_internal_handle());
			glm::ivec4* ptr = (glm::ivec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			glm::ivec4 result = *ptr;
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		//	const int total_probes = ddgiGRID.x * ddgiGRID.y * ddgiGRID.z;
			printf("total_probes %d\n", total_probes);
			printf("invalid probes: %d\n", result.y);
			printf("no_depth_needed probes: %d\n", result.x);

		}
		}

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	}


	if(do_flush_after)
		glFinish();	// finish here to get accurate time..
	float time = GetTime() - start;
	sys_print(Debug, "sfasdf asdf sadftime: %f\n", time);
	//{
	//
	//	CreateBufferArgs args;
	//	args.flags = GraphicsBufferUseFlags::BUFFER_USE_AS_STORAGE_READ;
	//	struct ProbeInfo {
	//		glm::vec4 pos_size;
	//	};
	//	args.size = 256 * sizeof(ProbeInfo);
	//	auto& objs = draw.scene.reflection_volumes.objects;
	//	std::vector<ProbeInfo> infos;
	//	for (int i = 0; i < objs.size(); i++) {
	//		auto& o = objs[i].type_;
	//		auto size = (o.boxmax - o.boxmin);
	//		float max_side = max(max(size.x, size.y), size.z);
	//
	//		glm::vec4 v = glm::vec4(objs[i].type_.probe_position, max_side * 0.5);	// fixme
	//		infos.push_back({ v });
	//	}
	//	buf = IGraphicsDevice::inst->create_buffer(args);
	//	buf->upload(infos.data(), infos.size() * sizeof(ProbeInfo));
	//}

	compute_avg_probe_value();

	ray_buffer->release();
	invalid_count_buf->release();

}
void draw_model_simple_no_material(Model* model);
#include "Render/ModelManager.h"
ConfigVar draw_real_grid("draw_real_grid", "2", CVAR_INTEGER|CVAR_UNBOUNDED, "");
void DdgiTesting::render_probes()
{
	if (myvolumes.size() == 0)
		return;

	//if (!verts) {
	//	execute();
	//	ASSERT(verts);
	//}
	auto& device = draw.get_device();

	auto set_composite_pass = [&]() {
		RenderPassState pass_state;
		pass_state.wants_color_clear = false;
		auto color_infos = {
			ColorTargetInfo(draw.tex.output_composite)
		};
		pass_state.color_infos = color_infos;
		pass_state.depth_info = draw.tex.scene_depth;
		IGraphicsDevice::inst->set_render_pass(pass_state);
	};
	set_composite_pass();

	RenderPipelineState state = RenderPipelineState();
	state.program = debug_probes;
	state.vao = g_modelMgr.get_vao_ptr(VaoType::Animated)->get_internal_handle();
	device.set_pipeline(state);

	Model* m = Model::load("sphere.cmdl");
	m->set_globally_referenced();

	device.bind_texture_ptr(0, probe_irradiance);
	set_shit_fuck();
	if(draw_real_grid.get_integer()==2){
		for (auto& vol : myvolumes) {
			auto ddgiGRID = vol.size_offset;
	device.shader().set_ivec3("vol_grid", ddgiGRID);
	device.shader().set_int("vol_offset", ddgiGRID.w);

		for (int x = 0; x < ddgiGRID.x; x++) {
			for (int y = 0; y < ddgiGRID.y; y++) {
				for (int z = 0; z < ddgiGRID.z; z++) {

					const int global_index = x + y * ddgiGRID.x + z * ddgiGRID.x * ddgiGRID.y + ddgiGRID.w;
					glm::vec3 ofs = glm::vec3(temp_probe_relocate_thing.at(global_index));

					glm::mat4 tr = glm::translate(glm::mat4(1), glm::vec3(x, y, z) * glm::vec3(vol.density) + glm::vec3(vol.origin_priority) + ofs);
					device.shader().set_mat4("Model", glm::scale(tr, glm::vec3(0.2)));
					device.shader().set_ivec3("probe_coord", { x,y,z });

					draw_model_simple_no_material(m);
				}
			}
		}
		}
	}

}

#include "Render/RenderGiManager.h"
void DdgiTesting::calculate_lum_for_spec()
{
	const int num = RenderGiManager::inst->get_num_cubemaps();
	if (num == 0) return;
	if (!probe_depth || !probe_irradiance) return;
	auto& device = draw.get_device();
	device.bind_texture(2, probe_irradiance->get_internal_handle());
	device.bind_texture(3, probe_depth->get_internal_handle());

	device.set_shader(lum_calc);
	set_shit_fuck();
	const int num_cubemaps = RenderGiManager::inst->get_num_cubemaps();
	device.shader().set_int("num_cubemaps", num_cubemaps);


	const int groups = glm::ceil(num / 64.f);
	printf("$$$$$$$$$$$$$$$$$$calculate_lum_for_spec %d\n",groups);

	IGraphicsBuffer* const cubemap_volume_buffer = RenderGiManager::inst->get_cubemap_volume_buffer();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, cubemap_volume_buffer->get_internal_handle());
	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

}




void DdgiTesting::load_the_gi(IGraphicsTexture* irrad, IGraphicsTexture* depth, std::vector<glm::vec4>& relocate, std::vector<DdgiVolumeGpu>& vols)
{
	const int width_probe_space = irrad->get_size().x / ddgiIRRADTILE;
	const int height_probe_space = irrad->get_size().y / ddgiIRRADTILE;
	ASSERT(width_probe_space == depth->get_size().x / ddgiDEPTHTILE);
	ASSERT(height_probe_space == depth->get_size().y / ddgiDEPTHTILE);

	this->myvolumes = std::move(vols);

	DdgiGlobals globals{};
	globals.atlas_x = width_probe_space;
	globals.atlas_y = height_probe_space;
	globals.num_volumes = myvolumes.size();
	globals.relocate_normal_dist = relocate_normal_dist;
	ddgi_globals->upload(&globals, sizeof(globals));
	ddgi_volumes->upload(myvolumes.data(), myvolumes.size() * sizeof(DdgiVolumeGpu));
	theglobals = globals;

	temp_probe_relocate_thing = std::move(relocate);
	ddgi_probe_relocation_offsets->upload(temp_probe_relocate_thing.data(), sizeof(glm::vec4) * temp_probe_relocate_thing.size());

	if (this->probe_irradiance) {
		this->probe_irradiance->release();
	}
	probe_irradiance = irrad;
	if (this->probe_depth) {
		this->probe_depth->release();
	}
	probe_depth = depth;


	compute_avg_probe_value();
}
void DdgiTesting::draw_lighting_fullres(IGraphicsTexture* ssao, bool for_cubemap_view)
{
	auto& device = draw.get_device();

	// pass already set by caller

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = shade_fs;
	state.blend = BlendState::ADD;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	draw_lighting_shared(ssao,for_cubemap_view);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}
ConfigVar r_ddgi_halfres_blend("r.ddgi_halfres_blend", "0.8", CVAR_FLOAT, "[0,1] blend input into ddgi temporal accumulation");
void DdgiTesting::draw_lighting_halfres(IGraphicsTexture* ssao)
{
	auto& device = draw.get_device();

	const glm::ivec2 texel_offset = get_half_res_offset();

	auto targets = {
			ColorTargetInfo(draw.tex.halfres_scene_color)
	};
	RenderPassState rp;
	rp.color_infos = targets;
	IGraphicsDevice::inst->set_render_pass(rp);

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = shade_fs_halfres;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	draw_lighting_shared(ssao,false);
	device.shader().set_ivec2("halfres_texel_offset", texel_offset);
	device.shader().set_bool("wants_half_res", true);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	{
		GPUSCOPESTART(temporal_upsample_ddgi);

		// now do temporal upsample
		auto targets2 = {
			ColorTargetInfo(draw.tex.ddgi_accum)
		};
		rp.color_infos = targets2;
		IGraphicsDevice::inst->set_render_pass(rp);

		state.blend = BlendState::OPAQUE;
		state.program = temporal_upsample;
		device.set_pipeline(state);

		draw.bind_texture_ptr(0, draw.tex.halfres_scene_color);
		draw.bind_texture_ptr(1, draw.tex.last_ddgi_accum);
		draw.bind_texture_ptr(2, draw.tex.scene_depth);
		draw.bind_texture_ptr(3, draw.tex.scene_motion);
		draw.bind_texture_ptr(4, draw.tex.last_scene_motion);

		// FIXME
		extern ConfigVar r_taa_blend;
		extern ConfigVar r_taa_flicker_remove;
		extern ConfigVar r_taa_reproject;
		extern ConfigVar r_taa_dilate_velocity;
		extern float taa_doc_mult;
		extern float taa_doc_vel_bias;
		extern float taa_doc_bias;
		extern float taa_doc_pow;

		auto the_shader = device.shader();
		the_shader.set_float("amt", r_ddgi_halfres_blend.get_float());
		the_shader.set_bool("remove_flicker", r_taa_flicker_remove.get_bool());
		the_shader.set_mat4("lastViewProj", draw.last_frame_main_view.viewproj);
		the_shader.set_bool("use_reproject", r_taa_reproject.get_bool());
		the_shader.set_float("doc_mult", taa_doc_mult);
		the_shader.set_float("doc_vel_bias", taa_doc_vel_bias);
		the_shader.set_float("doc_bias", taa_doc_bias);
		the_shader.set_float("doc_pow", taa_doc_pow);
		the_shader.set_bool("dilate_velocity", r_taa_dilate_velocity.get_bool());
		the_shader.set_ivec2("halfres_texel_offset", texel_offset);


		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	GPUSCOPESTART(ddgihalfres_apply);


	// now apply to scene
	auto targets3 = {
		ColorTargetInfo(draw.tex.scene_color)
	};
	rp.color_infos = targets3;
	IGraphicsDevice::inst->set_render_pass(rp);
	state.program = apply_halfres_accum_to_scene;
	state.blend = BlendState::ADD;
	device.set_pipeline(state);
	draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
	draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
	draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
	draw.bind_texture_ptr(3, draw.tex.scene_depth);
	draw.bind_texture_ptr(4, ssao);
	draw.bind_texture_ptr(5, draw.tex.ddgi_accum);
	device.shader().set_vec2("ssr_lum_range", ssr_lum_range);
	extern ConfigVar enable_ssr;
	if (enable_ssr.get_bool())
		draw.bind_texture_ptr(6, draw.tex.reflection_accum);
	else
		draw.bind_texture_ptr(6, draw.black_texture);

	
	set_reflection_uniforms();

	glDrawArrays(GL_TRIANGLES, 0, 3);



	// now swap
	std::swap(draw.tex.ddgi_accum, draw.tex.last_ddgi_accum);
	increment_half_res_offset_index();
}

void DdgiTesting::draw_lighting_shared(IGraphicsTexture* ssao, bool for_cubemap_view)
{
	auto& device = draw.get_device();


	theglobals.view_bias = view_bias;
	theglobals.normal_bias = normal_bias;
	ddgi_globals->upload(&theglobals, sizeof(theglobals));

	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, indirection->get_internal_handle());

	draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
	draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
	draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
	draw.bind_texture_ptr(3, draw.tex.scene_depth);


	draw.bind_texture_ptr(4, probe_irradiance);
	draw.bind_texture_ptr(5, probe_depth);
	draw.bind_texture_ptr(6, ssao);

	extern ConfigVar r_specular_ao_intensity;
	device.shader().set_bool("include_cubemaps", !for_cubemap_view && include_cubemaps);


	set_shit_fuck();

	device.shader().set_ivec3("selected_probe_pos", selected_probe);
	device.shader().set_float("irrad_mult", irrad_mult);
	device.shader().set_bool("wants_half_res", false);
	set_reflection_uniforms();
}

void DdgiTesting::set_reflection_uniforms()
{
	auto& device = draw.get_device();

	extern ConfigVar r_specular_ao_intensity;
	device.shader().set_float("specular_ao_intensity", r_specular_ao_intensity.get_float());
	device.shader().set_int("lum_adjust_mode", lum_adjust_mode);

	{
		IGraphicsTexture* const cubemap_array = RenderGiManager::inst->get_cubemap_array_texture();
		const int num_cubemaps = RenderGiManager::inst->get_num_cubemaps();
		IGraphicsBuffer* const cubemap_volume_buffer = RenderGiManager::inst->get_cubemap_volume_buffer();

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, cubemap_volume_buffer->get_internal_handle());
		draw.bind_texture_ptr(8, cubemap_array);
		device.shader().set_int("num_cubemaps", num_cubemaps);
	}

	draw.bind_texture(7, EnviornmentMapHelper::get().integrator.get_texture());
	draw.bind_texture(9, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());

}

void DdgiTesting::draw_lighting(IGraphicsTexture* ssao, bool for_cubemap_view)
{
	if (draw.scene.skylights.empty())
		return;//fixme
	//if (!verts) {
	//	execute();
	//	ASSERT(verts);
	//}

	//render_rt();
	//return;

	auto& device = draw.get_device();

	
	GPUFUNCTIONSTART;

	if (wants_half_res && !for_cubemap_view)
		draw_lighting_halfres(ssao);
	else
		draw_lighting_fullres(ssao, for_cubemap_view);
}
void DdgiTesting::render_rt()
{
	GPUFUNCTIONSTART;

	if (!verts) {
		build_world();
		ASSERT(verts);
	}
	auto& device = draw.get_device();
	auto set_composite_pass = [&]() {
		RenderPassState pass_state;
		pass_state.wants_color_clear = true;
		auto color_infos = {
			ColorTargetInfo(draw.tex.output_composite)
		};
		pass_state.color_infos = color_infos;
		IGraphicsDevice::inst->set_render_pass(pass_state);
	};
	set_composite_pass();

	RenderPipelineState state;
	state.program = raytrace_test;
	state.vao = draw.get_empty_vao();
	device.set_pipeline(state);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verts->get_internal_handle());
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indicies->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());


	glDrawArrays(GL_TRIANGLES, 0, 3);

}
