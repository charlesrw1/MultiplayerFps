#include "RaytraceTest.h"
#include "Render/DrawLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"


ConfigVar ddgi_size("ddgi_size", "10", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar ddgi_density("ddgi_density", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "probes/meter");

const int MAX_RAYS = 64;

const glm::ivec3 ddgiGRID{ 5,2,5 };
const glm::vec3 ddgiVolumeOrigin = glm::vec3(-2.5, 0, -2.5);

const int ddgiIRRADTILE = 8;
const int ddgiDEPTHTILE = 16;


DdgiTesting::DdgiTesting()
{
	raytrace_test = draw.get_prog_man().create_raster("fullscreenquad.txt", "rtF.txt");

	gather_shader = draw.get_prog_man().create_compute("gather_C.txt");
	trace_shader = draw.get_prog_man().create_compute("trace_C.txt");

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
Color32 get_color_of_material_for_export(const MaterialInstance* m);
#include "Assets/AssetDatabase.h"
void DdgiTesting::build_world()
{
	auto& objs = draw.scene.proxy_list;

	std::vector<Bounds> bounds;
	std::vector<int> all_indicies;
	std::vector<glm::vec4> all_verticies;



	int counter = 0;
	for (auto& _o : objs.objects) {
		auto& o = _o.type_.proxy;
		if (!o.model)
			continue;
		if (o.model->get_num_lods() == 0 || !o.model->has_lightmap_coords())
			continue;
		counter += 1;
		// upload transformed verts, indicies, add bounds

		const glm::mat4 transform = o.transform;
		auto rmd = o.model->get_raw_mesh_data();

		if (rmd->get_num_verticies(0) > 2000)
			continue;

		// lowest lod
		auto& lod = o.model->get_lod(o.model->get_num_lods() - 1);

		// for all parts
		for (int parti = lod.part_ofs; parti < lod.part_ofs+lod.part_count; parti++) {
			const int index_start = all_indicies.size();
			const int vertex_start = all_verticies.size();

			const auto& part = o.model->get_part(parti);
			auto material_inst = o.model->get_material(part.material_idx);

			if (!material_inst) continue;
			auto material = material_inst->impl.get();
			

			const int material_offset = material->gpu_buffer_offset == -1 ? 0 : material->get_material_index_from_buffer_ofs();

			const int part_index_start = part.element_offset / 2;
			const int part_index_count = part.element_count;
			const int base_vertex = part.base_vertex;
			const int num_verts = part.vertex_count;

			auto get_vertex = [&](int index) ->glm::vec3 {
				return transform * glm::vec4(rmd->get_vertex_at_index(base_vertex+index).pos, 1.0);
			};

			for (int i = 0; i < num_verts; i++) {
				all_verticies.push_back(glm::vec4(get_vertex(i), material_offset));
			}
			for (int i = 0; i < part_index_count; i+=3) {
				const int i0 = rmd->get_index_at_index(part_index_start+i);
				const int i1 = rmd->get_index_at_index(part_index_start+i+1);
				const int i2 = rmd->get_index_at_index(part_index_start+i+2);

				all_indicies.push_back(i0+vertex_start);
				all_indicies.push_back(i1+vertex_start);
				all_indicies.push_back(i2+vertex_start);
				const Bounds tri_bounds = get_tri_bounds(get_vertex(i0), get_vertex(i1), get_vertex(i2));
				bounds.push_back(tri_bounds);
			}
		}
	}
	BVH as = BVH::build(bounds, 4, PartitionStrategy::BVH_SAH);

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

		materialsdata.at(INDEX)=color32_to_vec4(get_color_of_material_for_export(allmats.at(i)));
	}



	CreateBufferArgs args;
	args.flags = GraphicsBufferUseFlags::BUFFER_USE_AS_STORAGE_READ;
	args.size = all_verticies.size() * sizeof(glm::vec4);
	verts = IGraphicsDevice::inst->create_buffer(args);
	verts->upload(all_verticies.data(), args.size);

	args.size = sizeof(int) * all_indicies.size();
	indicies = IGraphicsDevice::inst->create_buffer(args);
	indicies->upload(all_indicies.data(), args.size);


	args.size = sizeof(int) * as.indicies.size();
	references = IGraphicsDevice::inst->create_buffer(args);
	references->upload(as.indicies.data(), args.size);

	args.size = nodes.size() * sizeof(GPUBVHNode);
	this->nodes = IGraphicsDevice::inst->create_buffer(args);
	this->nodes->upload(nodes.data(), args.size);
	
	args.size = materialsdata.size() * sizeof(glm::vec4);
	this->materials = IGraphicsDevice::inst->create_buffer(args);
	this->materials->upload(materialsdata.data(), args.size);


	args.size = ddgiGRID.x * ddgiGRID.y * ddgiGRID.z * MAX_RAYS * sizeof(RayBufferStruct);
	this->ray_buffer = IGraphicsDevice::inst->create_buffer(args);

	CreateTextureArgs targs;
	const int tiles_wide = ddgiGRID.x * ddgiGRID.y;
	const int tiles_height = ddgiGRID.z;
	targs.width = tiles_wide * ddgiIRRADTILE;
	targs.height = tiles_height * ddgiIRRADTILE;

	targs.type = GraphicsTextureType::t2D;
	targs.num_mip_maps = 1;
	targs.format = GraphicsTextureFormat::r11f_g11f_b10f;
	targs.sampler_type = GraphicsSamplerType::LinearDefault;
	probe_irradiance = IGraphicsDevice::inst->create_texture(targs);

	auto handle = Texture::install_system("_ddgi");
	handle->update_specs_ptr(this->probe_irradiance);
	handle->type = Texture_Type::TEXTYPE_2D;

	targs.width = tiles_wide * ddgiDEPTHTILE;
	targs.height = tiles_height * ddgiDEPTHTILE;
	targs.format = GraphicsTextureFormat::rg16f;
	probe_depth = IGraphicsDevice::inst->create_texture(targs);
}

void DdgiTesting::execute()
{
	build_world();
	double start = GetTime();

	// run trace
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verts->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indicies->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ray_buffer->get_internal_handle());
	auto& device = draw.get_device();
	device.bind_texture(5,draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());
	device.set_shader(trace_shader);
	device.shader().set_vec3("volume_origin", ddgiVolumeOrigin);
	device.shader().set_vec3("volume_spacing", vec3(2,2,2));
	device.shader().set_ivec3("vol_grid", ddgiGRID);

	const int total_probes = ddgiGRID.x * ddgiGRID.y * ddgiGRID.z;
	const int groups = glm::ceil(total_probes / 64.f);

	glDispatchCompute(groups, 1, 1);

	// then run gather
	device.set_shader(gather_shader);
	device.shader().set_ivec3("vol_grid", ddgiGRID);


	glBindImageTexture(0, probe_irradiance->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);
	glCheckError();
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glDispatchCompute(groups, 1, 1);
	glCheckError();

	float time = GetTime() - start;
	sys_print(Debug, "time: %f\n",  time);
}

void DdgiTesting::render()
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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indicies->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());


	glDrawArrays(GL_TRIANGLES, 0, 3);

}
