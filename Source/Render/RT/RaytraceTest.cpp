#include "RaytraceTest.h"
#include "Render/DrawLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"

#include "Framework/MathLib.h"

ConfigVar ddgi_size("ddgi_size", "10", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar ddgi_density("ddgi_density", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "probes/meter");

const int MAX_RAYS = 256;

const glm::ivec3 ddgiGRID{50,8,50 };
const glm::vec3 ddgiVolumeOrigin = glm::vec3(-30, 0, -11);

const glm::vec3 ddgiDensity = vec3(2);

const int ddgiIRRADTILE = 8;
const int ddgiDEPTHTILE = 16;
using std::vector;

class OctTree
{
public:
	struct Node {
		std::vector<std::unique_ptr<Node>> subnodes;
		bool tri_intersects = false;
		Bounds bounds;
		bool forced_subdivide = false;
		bool get_has_subdivied() const {
			return !subnodes.empty();
		}
		void subdivide() {
			ASSERT(!get_has_subdivied());
			for (int i = 0; i < 8; i++) {
				subnodes.push_back(uptr<Node>(new Node));
			}
		}
	};
	vec3 origin = vec3(0.0);
	float size = 64.0;
	uptr<Node> root_node;
	int max_depth = 6;
	float min_leaf_size = 1.0f;// 2.0f;
	
	void insert_triangles(const vector<Bounds>& tri_bounds) {
		root_node = uptr<Node>(new Node);
		root_node->bounds = Bounds(origin-vec3(size), origin + vec3(size));
		
		for (int i = 0; i < tri_bounds.size(); i++) {
			insert_triangle(root_node.get(), tri_bounds[i], 0);
		}

	}
	
private:
	void insert_triangle(Node* node, const Bounds& tri_bounds, int depth) {
		float node_size = size / (1 << depth);
		if (node_size <= min_leaf_size || depth >= max_depth) {
			return;
		}
		
		if (!node->get_has_subdivied()) {
			node->subdivide();
			for (int i = 0; i < 8; i++) {
				node->subnodes[i]->bounds = get_child_bounds(node->bounds, i);
			}
		}
		for (int i = 0; i < 8; i++) {
			auto& sn = node->subnodes[i];
			sn->tri_intersects |= sn->bounds.intersect(tri_bounds);
		}

		
		for (int i = 0; i < 8; i++) {
			if (node->subnodes[i]->bounds.intersect(tri_bounds)) {
				insert_triangle(node->subnodes[i].get(), tri_bounds, depth + 1);
			}
		}
	}
	
 	
	Bounds get_child_bounds(const Bounds& parent, int child_idx) {
		vec3 half = (parent.bmax - parent.bmin) * 0.5f;
		vec3 offset = vec3(
			(child_idx & 1) ? half.x : 0,
			(child_idx & 2) ? half.y : 0,
			(child_idx & 4) ? half.z : 0
		);
		return Bounds(parent.bmin + offset, parent.bmin + offset + half);
	}
};


DdgiTesting::DdgiTesting()
{
	raytrace_test = draw.get_prog_man().create_raster("fullscreenquad.txt", "rtF.txt");

	gather_shader = draw.get_prog_man().create_compute("gather_C.txt");
		shade_fs = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeF.txt");
		shade_debug_fs = draw.get_prog_man().create_raster("fullscreenquad.txt", "ddgiShadeDebugF.txt");

	//	while (1) {
			trace_shader = draw.get_prog_man().create_compute("trace_C.txt");
	//	};
	debug_probes = draw.get_prog_man().create_raster("MeshSimpleV.txt", "MeshDebugProbeF.txt");

	get_best_cubemap_shader = draw.get_prog_man().create_compute("get_best_cubemap_C.txt");
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
struct PosAndValue {
	glm::vec3 v{};
	bool needs_depth = false;
};
static std::unordered_map<uint64_t, PosAndValue> hash_to_pos;
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

		// lowest lod
		auto& lod = o.model->get_lod(o.model->get_num_lods() - 1);

		// for all parts
		for (int parti = lod.part_ofs; parti < lod.part_ofs+lod.part_count; parti++) {
			const int index_start = all_indicies.size();
			const int vertex_start = all_verticies.size();

			const auto& part = o.model->get_part(parti);
			auto material_inst = o.model->get_material(part.material_idx);
			if (matoverride)
				material_inst = matoverride;

			if (!material_inst) continue;
			auto material = material_inst->impl.get();
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

		auto vec = color32_to_vec4(get_color_of_material_for_export(allmats.at(i)));
		auto linear = glm::pow(vec, glm::vec4(2.2));
		materialsdata.at(INDEX) = linear;
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
	probe_irradiance->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);

	auto handle = Texture::install_system("_ddgi");
	handle->update_specs_ptr(this->probe_irradiance);
	handle->type = Texture_Type::TEXTYPE_2D;

	targs.width = tiles_wide * ddgiDEPTHTILE;
	targs.height = tiles_height * ddgiDEPTHTILE;
	targs.format = GraphicsTextureFormat::rg16f;
	probe_depth = IGraphicsDevice::inst->create_texture(targs);
	probe_depth->sub_image_upload(0, 0, 0, targs.width, targs.height, 0, nullptr);


	handle = Texture::install_system("_ddgi_d");
	handle->update_specs_ptr(this->probe_depth);
	handle->type = Texture_Type::TEXTYPE_2D;

	args.size = ddgiGRID.x*ddgiGRID.y*ddgiGRID.z*4;
	this->probe_to_best_cubemap = IGraphicsDevice::inst->create_buffer(args);
	this->probe_to_best_cubemap->upload(nullptr, args.size);

	{
		indirection = IGraphicsDevice::inst->create_buffer(args);
		std::vector<int> indicies;
		const int count = ddgiGRID.x * ddgiGRID.y;
		Random r(13);
		for (int i = 0; i < count; i++) {
			indicies.push_back(r.RandI(0, count - 1));
		}
		indirection->upload(indicies.data(), indicies.size() * sizeof(int));

	};

	{
		OctTree tree;
		tree.insert_triangles(bounds);

		auto make_hash = [](glm::vec3 c) -> uint64_t {
			std::size_t hx = std::hash<int>()(int(c.x) * 73856093);
			std::size_t hy = std::hash<int>()(int(c.y) * 19349663);
			std::size_t hz = std::hash<int>()(int(c.z) * 83492791);
			return hx ^ hy ^ hz;
		};
		auto add_pos_to_hash = [&](glm::vec3 v, bool needs_depth) {
			auto& val = hash_to_pos[make_hash(v)];
			val.needs_depth |= needs_depth;
			val.v = v;
		};

		static MeshBuilder mb;
		mb.Begin();
	
		auto recurse = [&](auto&& func, OctTree::Node* n, int depth) -> void {
			if (!n->get_has_subdivied()) {
				if (depth == 6) {
					auto center = n->bounds.get_center();
					auto size = n->bounds.bmax - n->bounds.bmin;
					auto newmin = center - size * 0.25f;
					auto newmax = center + size * 0.25f;
					mb.PushLineBox(newmin, newmax, COLOR_RED);
				}
			}
			else {
				for (int i = 0; i < 8; i++)
					func(func, n->subnodes.at(i).get(),depth+1);
			}

		};
		recurse(recurse, tree.root_node.get(),0);

		auto recurse2 = [&](auto&& func, OctTree::Node* n, int depth) -> void {
				if (1) {
					const bool needs_depth = n->tri_intersects&&depth == 6;	// lowest level
					auto center = n->bounds.get_center();
					auto size = n->bounds.bmax - n->bounds.bmin;
					auto newmin = center - size * 0.5f;
					auto newmax = center + size * 0.5f;
					// add 8 corners
					for (int i = 0; i < 8; i++) {
						glm::vec3 corner = glm::vec3(
							(i & 1) ? newmax.x : newmin.x,
							(i & 2) ? newmax.y : newmin.y,
							(i & 4) ? newmax.z : newmin.z
						);
						add_pos_to_hash(corner, needs_depth);
					}
				}
			if (!n->get_has_subdivied()) {
			}
			else {
				for (int i = 0; i < 8; i++)
					func(func, n->subnodes.at(i).get(), depth + 1);
			}
		};
		recurse2(recurse2, tree.root_node.get(), 0);
		printf("$$$$$$$ probes: %d\n", int(hash_to_pos.size()));
		int probes_that_need_depth = 0;
		for (auto& [key, val] : hash_to_pos)
			probes_that_need_depth += val.needs_depth;
		printf("$$$$$ needs depth: %d\n", probes_that_need_depth);

		mb.End();

		//auto handle = idraw->get_scene()->register_meshbuilder();
		MeshBuilder_Object obj;
		obj.meshbuilder = &mb;
		obj.visible = true;
		obj.use_background_color = true;
		
		//idraw->get_scene()->update_meshbuilder(handle, obj);
	}
}
#include "imgui.h"
static float irrad_mult = 1.0;
void ddgi_debugmenu() {
	auto self = draw.ddgi.get();
	ImGui::InputInt3("select", &self->selected_probe.x);
	self->selected_probe = glm::clamp(self->selected_probe, glm::ivec3(0), ddgiGRID - glm::ivec3(1));

	ImGui::DragFloat("irradmul", &irrad_mult, 0.01);
}
ADD_TO_DEBUG_MENU(ddgi_debugmenu);

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

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, draw.buf.lighting_uniforms->get_internal_handle());


	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, indirection->get_internal_handle());

	auto& device = draw.get_device();



	IGraphicsBuffer* invalid_count_buf{};
	invalid_count_buf = IGraphicsDevice::inst->create_buffer({});
	int counter_num = 0;
	invalid_count_buf->upload(&counter_num, sizeof(int));
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, invalid_count_buf->get_internal_handle());

	Random r(13);
	bool do_irrad_calcs = false;
	for (int i = 0; i < 8; i++) {
		device.bind_texture(5, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());

		device.bind_texture(2, probe_irradiance->get_internal_handle());
		device.bind_texture(3, probe_depth->get_internal_handle());



		device.set_shader(trace_shader);
		device.shader().set_bool("do_irrad_calcs", do_irrad_calcs);
		device.shader().set_vec3("volume_origin", ddgiVolumeOrigin);
		device.shader().set_vec3("volume_spacing", ddgiDensity);
		device.shader().set_ivec3("vol_grid", ddgiGRID);
		device.shader().set_float("ray_sample_randomness", r.RandF(0,TWOPI));
		device.shader().set_int("num_lights", draw.scene.light_list.objects.size());

		const int total_probes = ddgiGRID.x * ddgiGRID.y * ddgiGRID.z;
		const int groups = glm::ceil(total_probes / 64.f);

		printf("trace %d\n", i);
		glDispatchCompute(groups, 1, 1);

		// then run gather
		device.set_shader(gather_shader);
		device.shader().set_ivec3("vol_grid", ddgiGRID);
		device.shader().set_vec3("volume_spacing", ddgiDensity);
		device.shader().set_int("num_runs_so_far", i);



		glBindImageTexture(0, probe_irradiance->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R11F_G11F_B10F);

		glBindImageTexture(1, probe_depth->get_internal_handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

		glCheckError();
		//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		printf("gather %d\n", i);
		glDispatchCompute(groups, 1, 1);
		glCheckError();

		if (i == 0) {
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, invalid_count_buf->get_internal_handle());
			int* ptr = (int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			int result = *ptr;
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			printf("invalid probes: %d\n", result);
		}


		float time = GetTime() - start;
		sys_print(Debug, "time: %f\n", time);
		do_irrad_calcs = true;
	}
	{
		CreateBufferArgs args;
		args.flags = GraphicsBufferUseFlags::BUFFER_USE_AS_STORAGE_READ;
		struct ProbeInfo {
			glm::vec4 pos_size;
		};
		args.size = 256 * sizeof(ProbeInfo);
		auto& objs = draw.scene.reflection_volumes.objects;
		std::vector<ProbeInfo> infos;
		for (int i = 0; i < objs.size(); i++) {
			auto& o = objs[i].type_;
			auto size = (o.boxmax - o.boxmin);
			float max_side = max(max(size.x, size.y), size.z);

			glm::vec4 v = glm::vec4(objs[i].type_.probe_position, max_side*0.5);	// fixme
			infos.push_back({ v });
		}
		buf = IGraphicsDevice::inst->create_buffer(args);
		buf->upload(infos.data(), infos.size() * sizeof(ProbeInfo));


		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, buf->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, probe_to_best_cubemap->get_internal_handle());


		// find best cubemaps
		device.set_shader(get_best_cubemap_shader);
		device.shader().set_vec3("volume_origin", ddgiVolumeOrigin);
		device.shader().set_vec3("volume_spacing", ddgiDensity);
		device.shader().set_ivec3("vol_grid", ddgiGRID);
		device.shader().set_int("num_cubemap_volumes", infos.size());
		const int total_probes = ddgiGRID.x * ddgiGRID.y * ddgiGRID.z;
		const int groups = glm::ceil(total_probes / 64.f);


		glDispatchCompute(groups, 1, 1);



	//	buf->release();
	}


}
void draw_model_simple_no_material(Model* model);
#include "Render/ModelManager.h"
ConfigVar draw_real_grid("draw_real_grid", "2", CVAR_INTEGER|CVAR_UNBOUNDED, "");
void DdgiTesting::render_probes()
{
	if (!verts) {
		execute();
		ASSERT(verts);
	}
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
	device.bind_texture_ptr(0, probe_irradiance);
	device.shader().set_ivec3("vol_grid", ddgiGRID);

	if (draw_real_grid.get_integer()==1) {
		for (auto& [_, p] : hash_to_pos) {
			glm::mat4 tr = glm::translate(glm::mat4(1), p.v);
			device.shader().set_mat4("Model", glm::scale(tr, glm::vec3(0.2)));
			device.shader().set_ivec3("probe_coord", { 0,0,0 });

			draw_model_simple_no_material(m);
		}
	}
	else if(draw_real_grid.get_integer()==2){
		for (int x = 0; x < ddgiGRID.x; x++) {
			for (int y = 0; y < ddgiGRID.y; y++) {
				for (int z = 0; z < ddgiGRID.z; z++) {
					glm::mat4 tr = glm::translate(glm::mat4(1), glm::vec3(x, y, z) * ddgiDensity + ddgiVolumeOrigin);
					device.shader().set_mat4("Model", glm::scale(tr, glm::vec3(0.2)));
					device.shader().set_ivec3("probe_coord", { x,y,z });

					draw_model_simple_no_material(m);
				}
			}
		}
	}

}
void DdgiTesting::draw_lighting(IGraphicsTexture* ssao, bool for_cubemap_view)
{
	if (!verts) {
		execute();
		ASSERT(verts);
	}

	//render_rt();
	//return;

	auto& device = draw.get_device();

	
	GPUFUNCTIONSTART;


	// pass already set by caller
	
	// auto set_composite_pass2 = [&]() {
	//	RenderPassState pass_state;
	//	pass_state.wants_color_clear = false;
	//	auto color_infos = {
	//		ColorTargetInfo(draw.tex.output_composite)
	//	};
	//	pass_state.color_infos = color_infos;
	//	IGraphicsDevice::inst->set_render_pass(pass_state);
	//};
	//set_composite_pass2();


	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = shade_fs;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, buf->get_internal_handle());

	draw.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
	draw.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
	draw.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
	draw.bind_texture_ptr(3, draw.tex.scene_depth);

	
	draw.bind_texture_ptr(4, probe_irradiance);
	draw.bind_texture_ptr(5, probe_depth);
	draw.bind_texture_ptr(6, ssao);

	//draw.bind_texture(7, Texture::load("topdown_gi.png")->get_internal_render_handle());
	//draw.bind_texture(8, Texture::load("bottom_gi.png")->get_internal_render_handle());


	// boxes (with pos and size)
	// trace against them, find best 1 box
	extern ConfigVar r_specular_ao_intensity;
	device.shader().set_bool("include_cubemaps", !for_cubemap_view);

	draw.bind_texture(7, EnviornmentMapHelper::get().integrator.get_texture());
	draw.bind_texture(8, draw.scene.cubemap_array->get_internal_handle());
	draw.bind_texture(9, draw.scene.skylights.at(0).skylight.generated_cube->get_internal_render_handle());


	draw.shader().set_float("specular_ao_intensity", r_specular_ao_intensity.get_float());

	device.shader().set_vec3("volume_origin", ddgiVolumeOrigin);
	device.shader().set_vec3("volume_spacing", ddgiDensity);
	device.shader().set_ivec3("vol_grid", ddgiGRID);
	device.shader().set_ivec3("selected_probe_pos", selected_probe);
	device.shader().set_float("irrad_mult", irrad_mult);
	device.shader().set_int("num_cubemaps", draw.scene.reflection_volumes.objects.size());


	// fullscreen shader, no vao used
	glDrawArrays(GL_TRIANGLES, 0, 3);

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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indicies->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, nodes->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, references->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, materials->get_internal_handle());


	glDrawArrays(GL_TRIANGLES, 0, 3);

}
