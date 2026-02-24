#include "GpuCullingTest.h"

GpuCullingTest::GpuCullingTest()
{
	model_data_buffer = IGraphicsDevice::inst->create_buffer({});
	object_buffer = IGraphicsDevice::inst->create_buffer({});
	multidraw_buffer = IGraphicsDevice::inst->create_buffer({});
	objindirect = IGraphicsDevice::inst->create_buffer({});
	matindirect = IGraphicsDevice::inst->create_buffer({});
	cull_data = IGraphicsDevice::inst->create_buffer({});
	cull_compute = draw.get_prog_man().create_compute("CullCompute.txt");
	build_pyramid = draw.get_prog_man().create_compute("DepthPyramidC.txt");
	cpu_vis_array_to_mdi = draw.get_prog_man().create_compute("cpu_vis_to_mdi.txt");
	debug_overlays  = draw.get_prog_man().create_raster("fullscreenquad.txt", "debugCull.txt");
	vis_bitarray = IGraphicsDevice::inst->create_buffer({});

	Texture::install_system("_depth_pyramid");

	glGenSamplers(1, &hiZSampler);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(hiZSampler, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MIN);


}

GpuCullingTest::~GpuCullingTest()
{
}
GpuCullingTest* GpuCullingTest::inst = nullptr;
#include "ModelManager.h"
#include "Frustum.h"
#include <algorithm>
ConfigVar skip_obj_upload("skip_obj_upload", "0", CVAR_BOOL, "");
#include "imgui.h"
static bool do_occlusion_culling = true;
static bool update_depth_pyramid = true;
static bool draw_debug_overlay = false;
static int lod_bias = 0;
static bool output_depth = false;
static float radius_bias = 1.0;
static glm::vec3 debug_pos{};
static float debug_radius = 1.f;
#include "LevelEditor/EditorDocLocal.h"
#include "GameEngineLocal.h"
void occ_cul_menu()
{
	ImGui::Checkbox("do_occlusion_culling", &do_occlusion_culling);
	ImGui::Checkbox("update_depth_pyramid", &update_depth_pyramid);
	ImGui::Checkbox("draw_debug_overlay", &draw_debug_overlay);
	ImGui::InputInt("lod_bias", &lod_bias);
	ImGui::Checkbox("output_depth", &output_depth);
	ImGui::InputFloat("radius_bias", &radius_bias);
	if (ImGui::Button("to selected")) {
		auto doc = (EditorDoc*)eng_local.editorState->get_tool();
		if (doc->selection_state->has_only_one_selected()) {
			auto only = doc->selection_state->get_only_one_selected();
			auto mesh = only->get_component<MeshComponent>();
			auto& objs = draw.scene.proxy_list.objects;
			for (auto &[o, h] : objs) {
				if (h.proxy.owner == mesh) {
					debug_pos = h.bounding_sphere_and_radius;
					debug_radius = h.bounding_sphere_and_radius.a;
				}
			}
		}
	}

}
ADD_TO_DEBUG_MENU(occ_cul_menu);
void GpuCullingTest::debug_overlay()
{
	if (!draw_debug_overlay)
		return;

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
	state.program = debug_overlays;
	state.vao = draw.get_empty_vao();
	device.set_pipeline(state);
	glBindBufferBase(GL_UNIFORM_BUFFER, 5, cull_data->get_internal_handle());
	device.shader().set_int("lod_bias", lod_bias);
	device.shader().set_bool("output_depth", output_depth);

	device.shader().set_vec4("debug_pos", glm::vec4(debug_pos,debug_radius));


	device.bind_texture_ptr(0, depth_pyramid);
	glBindSampler(0, hiZSampler);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindSampler(0, 0);

}
void GpuCullingTest::copy_cpu(Render_Lists_Gpu_Culled& list)
{
	GPUSCOPESTART(copy_cpu);

	auto& device = draw.get_device();

	device.set_shader(cpu_vis_array_to_mdi);
	const int co_size = cull.num_objects;
	const int groups = glm::ceil(int(co_size) / 256.f);

	device.shader().set_int("num_objects", list.obj_count);


	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, list.gpu_command_list);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,list.glinstance_to_instance);
	glBindBufferBase(GL_UNIFORM_BUFFER, 5, cull_data->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, vis_bitarray->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, list.inst_to_obj->get_internal_handle());

	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
void GpuCullingTest::build_data()
{
	// fill model data buffer
	// fill mdi buffer (+ sort)
	
	GPUSCOPESTART(gpu_cull);


	if(update_depth_pyramid)
	{


		const auto& viewsetup = draw.current_frame_view;
		int v_w = viewsetup.width/2;
		int v_h = viewsetup.height/2;
		if (depth_size.x != v_w || depth_size.y != v_h)
			init_depth_pyramid(v_w, v_h);
		downsample_depth();
	}

	
	std::vector<gpu::DrawElementsIndirectCommand> commands;
	std::vector<int> material_indirection;
	std::vector<const Model*> cmd_to_model;


	cmd_mats.clear();

	auto make_key = [&](int i) -> draw_call_key {
		draw_call_key k{};
		const MaterialInstance* this_mat = cmd_mats[i];
		if (!this_mat)
			return k;
		auto parent = this_mat->impl->get_master_impl();
		if (!parent)
			return k;

		k.backface = parent->backface;
		k.blending = (uint64_t)parent->blend;
		k.mesh = cmd_to_model[i]->get_uid();
		k.texture = cmd_mats[i]->impl->texture_id_hash;
		k.shader = matman.get_mat_shader(
			nullptr, cmd_mats[i],
			0
		);
		return k;
	};

	std::unordered_map<Model*, int> mod_to_ofs;
	static std::unordered_map<const Model*, int> models_used;
	{
		GPUSCOPESTART(mod_data);

		std::vector<int> mod_data_buf;
		const hash_set<Model>& all_models = g_modelMgr.get_all_models();
		for (auto m : all_models) {
			const int start = mod_data_buf.size();
			mod_to_ofs[(Model*)m] = mod_data_buf.size();
			const int num_lods = m->get_num_lods();
			mod_data_buf.push_back(num_lods);
			for (int i = 0; i < num_lods; i++) {
				mod_data_buf.push_back(m->get_lod(i).part_ofs);
				mod_data_buf.push_back(m->get_lod(i).part_count);
				const float f = m->get_lod(i).end_percentage;
				mod_data_buf.push_back(*((int*)&f));
			}
			const int num_parts = m->get_num_parts();
			for (int i = 0; i < num_parts; i++) {
				const int mat = m->get_part(i).material_idx;
				const MaterialInstance* matp = m->get_material(mat);

				auto& part = m->get_part(i);
				gpu::DrawElementsIndirectCommand cmd{};
				cmd.baseVertex = part.base_vertex + m->get_merged_vertex_ofs();
				cmd.count = part.element_count;
				cmd.firstIndex = part.element_offset + m->get_merged_index_ptr();
				cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

				// Important! Set primCount to 0 because visible instances will increment this
				cmd.primCount = 0;// meshb.count;
				cmd.baseInstance = 0;

				mod_data_buf.push_back(commands.size());

				material_indirection.push_back(matp->impl->gpu_buffer_offset);
				commands.push_back(cmd);
				cmd_to_model.push_back(m);
				cmd_mats.push_back(matp);
			}
		}

		// sort the commands around
		{
			std::vector<int> sorted;
			
			for (int i = 0; i < cmd_mats.size(); i++)
				sorted.push_back(i);
			std::sort(sorted.begin(), sorted.end(),
				[&](int a, int b) -> bool {
			
					return make_key(a).as_uint64() < make_key(b).as_uint64();
			
				});

			auto copiedmats = cmd_mats;
			for (int i = 0; i < sorted.size(); i++)
				cmd_mats[i] = copiedmats[sorted[i]];
			auto copiedmods = cmd_to_model;
			for (int i = 0; i < sorted.size(); i++)
				cmd_to_model[i] = copiedmods[sorted[i]];
			auto copiedmi = material_indirection;
			for (int i = 0; i < sorted.size(); i++)
				material_indirection[i] = copiedmi[sorted[i]];
			auto copcommands = commands;
			for (int i = 0; i < sorted.size(); i++)
				commands[i] = copcommands[sorted[i]];

			std::vector<int> inv_sorted(sorted.size());
			for (int i = 0; i < sorted.size(); i++) {
				inv_sorted[sorted[i]] = i;
			}

			// must adjust model index
			for (auto& [mod, ofs] : mod_to_ofs) {
				const int num_lods = mod_data_buf.at(ofs);
				for (int lod = 0; lod < num_lods; lod++) {
					const int part_ofs = mod_data_buf.at(ofs + 1 + lod * 3);
					const int part_count = mod_data_buf.at(ofs + 1 + lod * 3+1);
					for (int part = 0; part < part_count; part++) {
						const int index = ofs + 1 + 3 * num_lods + part_ofs + part;
						const int command_prev_index = mod_data_buf.at(index);
						const int remapped = inv_sorted.at(command_prev_index);
						mod_data_buf.at(index) = remapped;
					}
				}
			}
		}
		model_data_buffer->upload(mod_data_buf.data(), mod_data_buf.size() * sizeof(int));

	}
	if (!skip_obj_upload.get_bool()) {
		models_used.clear();
		auto& all_objs = draw.scene.proxy_list.objects;
		// first fill object buffer

		std::vector<CullObject> cos;
		int index = 0;
		for (auto& [h, o] : all_objs) {
			if (o.proxy.ignore_in_baking) {
				CullObject co{};
				co.bounds_sphere = o.bounding_sphere_and_radius;
				const int ofs = mod_to_ofs[o.proxy.model];;
				co.model_ofs = glm::ivec4(ofs, index, 0, 0);
				cos.push_back(co);
				models_used[o.proxy.model] += 1;
			}
			index += 1;
		}

		// cpu side objects
		cull.cpu_obj_offset = cos.size();
		for (auto& [h, o] : all_objs) {		
			CullObject co{};
			co.bounds_sphere = o.bounding_sphere_and_radius;
			co.model_ofs = glm::ivec4(-1, 0, 0, 0);
			cos.push_back(co);
			index += 1;
		}



		object_buffer->upload(cos.data(), cos.size() * sizeof(CullObject));
		cull.num_objects = cos.size();

		const int bytes_needed = glm::round(cull.num_objects / 8.f);
		vis_bitarray->upload(nullptr, bytes_needed);
		uint32 value = 0;
		glClearNamedBufferData(vis_bitarray->get_internal_handle(),
			GL_R32UI,
			GL_RED_INTEGER,
			GL_UNSIGNED_INT,
			&value);
	}

	// set base instances
	ASSERT(cmd_to_model.size() == commands.size() && commands.size() == material_indirection.size());
	int base_inst_sum = 0;
	for (int i = 0; i < cmd_to_model.size(); i++) {
		const Model* model = cmd_to_model[i];
		commands[i].baseInstance = base_inst_sum;
		base_inst_sum += models_used[model] + 100;
	}
	matindirect->upload(material_indirection.data(), material_indirection.size() * sizeof(int));
	if (!skip_obj_upload.get_bool()) {
		objindirect->upload(nullptr, sizeof(int) * base_inst_sum);
	}
	multidraw_buffer->upload(commands.data(), commands.size() * sizeof(gpu::DrawElementsIndirectCommand));


	if(update_depth_pyramid)
	{
		const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
		const float inv_two_times_tanfov_2 = inv_two_times_tanfov * inv_two_times_tanfov;
		cull.inv_two_times_tanfov_2 = inv_two_times_tanfov_2;
		auto& vs = draw.current_frame_view;
		cull.camera_origin =glm::vec4( vs.origin,1);

		Frustum frustum;
		build_a_frustum_for_perspective(frustum, draw.current_frame_view);
		cull.frustum_up = frustum.top_plane;
		cull.frustum_down = frustum.bot_plane;
		cull.frustum_l = frustum.left_plane;
		cull.frustum_r = frustum.right_plane;

		cull.near = vs.near;
		cull.pyramid_width = depth_size.x;
		cull.pyramid_height = depth_size.y;
		
		const float aratio = vs.width / (float)vs.height;
		const float halfVSide =  tanf(vs.fov * .5f);
		const float halfHSide = halfVSide * aratio;
		cull.p00 = 1 / halfHSide;
		cull.p11 = 1 / halfVSide;

		cull.view = prev_view;
		prev_view = vs.view;
		cull_data->upload(&cull, sizeof(CullData));
	}

	{
		GPUSCOPESTART(cull_step);

		auto& device = draw.get_device();

		device.set_shader(cull_compute);
		const int co_size = cull.num_objects;
		const int groups = glm::ceil(int(co_size) / 256.f);

		device.shader().set_bool("occlusion_cull", do_occlusion_culling);

		device.bind_texture_ptr(0, depth_pyramid);
		glBindSampler(0,hiZSampler);
		device.shader().set_int("lod_bias", lod_bias);
		device.shader().set_float("radius_bias", radius_bias);


		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, multidraw_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, objindirect->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, model_data_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, object_buffer->get_internal_handle());
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, cull_data->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, vis_bitarray->get_internal_handle());


		glDispatchCompute(groups, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glBindSampler(0,0);

	}

	// set up mesh batches (groups of drawelements commands, 1 multidraw command)
	{
		batches.clear();

		Multidraw_Batch batch;
		batch.first = 0;
		batch.count = 1;

		const Model* batch_model = cmd_to_model.at(0);
		auto batch_sort_key = make_key(0);

		for (int i = 1; i < commands.size(); i++)
		{
			
			const Model* this_model = cmd_to_model.at(i);
			auto this_sort_key = make_key(i);

			bool batch_this = false;

			bool same_layer = batch_sort_key.layer == this_sort_key.layer;
			bool same_vao = batch_sort_key.vao == this_sort_key.vao;
			bool same_material = batch_sort_key.texture == this_sort_key.texture;
			bool same_shader = batch_sort_key.shader == this_sort_key.shader;
			bool same_other_state = batch_sort_key.blending == this_sort_key.blending
				&& batch_sort_key.backface == this_sort_key.blending;

			if (true) {
				if (same_vao && same_material && same_other_state && same_shader && same_layer)
					batch_this = true;	// can batch with different meshes
				else
					batch_this = false;

			}
			else {// pass==DEPTH
				// can batch across texture changes as long as its not alpha tested
				if (same_shader && same_vao && same_other_state)
					batch_this = true;
				else
					batch_this = false;
			}

			if (batch_this) {
				batch.count += 1;
			}
			else {
				batches.push_back(batch);
				batch.count = 1;
				batch.first = i;

				batch_model = this_model;
				batch_sort_key = this_sort_key;
			}
		}

		batches.push_back(batch);
	}
}

void setup_batch2(const MaterialInstance* mat, const int offset)
{

	const program_handle program = matman.get_mat_shader(
		nullptr, mat,
		0
	);
	const BlendState blend = mat->get_master_material()->blend;
	const bool show_backface = false;

	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(VaoType::Lightmapped);

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface;
	state.blend = blend;
	state.depth_testing = true;
	//state.depth_writes = depth_write_enabled;
	state.depth_writes = !mat->get_master_material()->is_translucent();
	//state.depth_less_than = depth_less_than_op;
	draw.get_device().set_pipeline(state);


	draw.shader().set_int("indirect_material_offset", offset);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = 0;// t->gl_id;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}
extern const GLenum MODEL_INDEX_TYPE_GL;
extern ConfigVar test_ignore_bake;
void GpuCullingTest::dodraw()
{
	if (!test_ignore_bake.get_bool())
		return;
	

	IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();
	auto& scene = draw.scene;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_render_instance_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, material_buffer->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, objindirect->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, matindirect->get_internal_handle());

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, multidraw_buffer->get_internal_handle());

	int offset = 0;
	const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
	for (int i = 0; i < batches.size(); i++) {
		const int count = batches.at(i).count;
		const int mat_ofs = batches.at(i).first;
		//const int count = 1;// list.command_count[i];
		const int incr = count;// pass.batches[i].count;
		if (count != 0) {

			setup_batch2(cmd_mats[mat_ofs], offset);

			const GLenum index_type = MODEL_INDEX_TYPE_GL;

			void* indirect_ptr = nullptr;
		
			indirect_ptr = (void*)(int64_t(offset * DEIcmdSz));

			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				index_type,
				indirect_ptr,
				count,
				sizeof(gpu::DrawElementsIndirectCommand)
			);

		}
		offset += incr;
	}
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}
// vkguide
uint32_t previousPow2(uint32_t v)
{
	uint32_t r = 1;

	while (r * 2 < v)
		r *= 2;

	return r;
}
uint32_t getImageMipLevels(uint32_t width, uint32_t height)
{
	uint32_t result = 1;

	while (width > 1 || height > 1)
	{
		result++;
		width /= 2;
		height /= 2;
	}

	return result;
}
void GpuCullingTest::init_depth_pyramid(int w, int h)
{
	depth_size = { w,h };
	if (depth_pyramid)
		depth_pyramid->release();
	CreateTextureArgs args;
	args.num_mip_maps = Texture::get_mip_map_count(w, h);
	args.width = w;
	args.height = h;
	//previousPow2(w*2)
	//args.num_mip_maps = getImageMipLevels()
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::DepthPyramid;
	args.format = GraphicsTextureFormat::r32f;

	depth_pyramid = IGraphicsDevice::inst->create_texture(args);

	auto t = Texture::load("_depth_pyramid");
	t->update_specs_ptr(depth_pyramid);
	t->type = Texture_Type::TEXTYPE_2D;
}
// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
static bool projectSphere(vec3 C, float r, float znear, float P00, float P11,  vec4& aabb)
{
	if (C.z + r < znear)
		return false;

	vec2 cx = -glm::vec2(C.x,C.z);
	vec2 vx = vec2(sqrt(dot(cx, cx) - r * r), r);
	vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
	vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

	vec2 cy = -glm::vec2(C.y,C.z);
	vec2 vy = vec2(sqrt(dot(cy, cy) - r * r), r);
	vec2 miny = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
	vec2 maxy = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

	aabb = vec4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
	aabb = glm::vec4(aabb.x,aabb.w,aabb.z,aabb.y) * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f); // clip space -> uv space

	return true;
}
#include "UI/GUISystemPublic.h"
void GpuCullingTest::downsample_depth()
{
	GPUSCOPESTART(downsample);

	draw.get_device().set_shader(build_pyramid);
	const int levels = Texture::get_mip_map_count(depth_size.x, depth_size.y);
	int width = depth_size.x;
	int height = depth_size.y;
			//glBindSampler(0, hiZSampler);
	for (int level = 0; level < levels; level++) {
		//glBindImageTexture()
		glBindImageTexture(1, depth_pyramid->get_internal_handle(), level, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
		if (level == 0)
			draw.get_device().bind_texture_ptr(0, draw.tex.scene_depth);
		else {
			draw.get_device().bind_texture_ptr(0, depth_pyramid);
		}


		int groups_x = glm::ceil(width / 32.f);
		int groups_y = glm::ceil(height / 32.f);
		draw.shader().set_float("width", width);
		draw.shader().set_float("height", height);
		const int level_to_sample = level == 0 ? 0 : level - 1;
		draw.shader().set_int("level", level_to_sample);


		glDispatchCompute(groups_x, groups_y, 1);


		width /= 2.0;
		height /= 2.0;
		width = glm::max(width, 1);
		height = glm::max(height, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
			GL_TEXTURE_FETCH_BARRIER_BIT);
	}

			glBindSampler(0, 0);
	if(0)
	{
		glm::vec3 center = glm::vec3(0, 5, 0);
		float radius = 1.0;
		vec3 vs_center = (cull.view * vec4(center, 1.0));
		//vs_center.y *= -1;
		vs_center.z *= -1;	// [0,+inf]
		vec4 aabb;
		if (projectSphere(vs_center, radius, cull.near, cull.p00, cull.p11, aabb)) {
			float width = (aabb.z - aabb.x);
			float height = (aabb.w - aabb.y);

			RectangleShape shape;
			auto& vs = draw.get_current_frame_vs();
			shape.rect.w = vs.width * width;
			shape.rect.h = vs.height * height;
			shape.rect.x = aabb.x * vs.width;
			shape.rect.y = aabb.y * vs.height;

				

			UiSystem::inst->window.draw(shape);

		}
	}
}

