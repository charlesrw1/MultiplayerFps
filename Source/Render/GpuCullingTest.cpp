#include "GpuCullingTest.h"

GpuCullingTest::GpuCullingTest()
{

	matindirect = IGraphicsDevice::inst->create_buffer({});
	cull_data = IGraphicsDevice::inst->create_buffer({});
	cull_compute = draw.get_prog_man().create_compute("CullCompute.txt");
	cull_compute_cascade = draw.get_prog_man().create_compute("CullCompute.txt","SHADOW_CASCADE");

	build_pyramid = draw.get_prog_man().create_compute("DepthPyramidC.txt");
	cpu_vis_array_to_mdi = draw.get_prog_man().create_compute("cpu_vis_to_mdi.txt");
	debug_overlays  = draw.get_prog_man().create_raster("fullscreenquad.txt", "debugCull.txt");
	vis_bitarray = IGraphicsDevice::inst->create_buffer({});
	zero_instances_mdi = draw.get_prog_man().create_compute("zero_instances_mdi.txt");
	compaction = draw.get_prog_man().create_compute("compact_mdi.txt");


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
bool do_occlusion_culling = true;
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
// asdfa
static int blah = 0;
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
	
}
void GpuCullingTest::do_cull_for_scene(const GpuCullInput& input, Phase pass)
{
	Frustum frustum;
	build_a_frustum_for_perspective(frustum, draw.current_frame_view);
	cull.frustum_up = frustum.top_plane;
	cull.frustum_down = frustum.bot_plane;
	cull.frustum_l = frustum.left_plane;
	cull.frustum_r = frustum.right_plane;
	do_cull(input, pass, false, frustum);

}
extern ConfigVar r_force_lod;
void GpuCullingTest::do_cull(const GpuCullInput& input, Phase pass, bool is_for_shadow, Frustum frustum)
{
	if (cull.num_objects <= 0)
		return;

	// pass1: testing against last frame
	// pass2: testing against cur frame

	if (update_depth_pyramid)
	{
		const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
		const float inv_two_times_tanfov_2 = inv_two_times_tanfov * inv_two_times_tanfov;
		cull.inv_two_times_tanfov_2 = inv_two_times_tanfov_2;
		auto& vs = draw.current_frame_view;
		cull.camera_origin = glm::vec4(vs.origin, 1);

	
		cull.frustum_up = frustum.top_plane;
		cull.frustum_down = frustum.bot_plane;
		cull.frustum_l = frustum.left_plane;
		cull.frustum_r = frustum.right_plane;

		cull.near = vs.near;
		cull.pyramid_width = actual_depth_size.x;
		cull.pyramid_height = actual_depth_size.y;

		const float aratio = vs.width / (float)vs.height;
		const float halfVSide = tanf(vs.fov * .5f);
		const float halfHSide = halfVSide * aratio;
		cull.p00 = 1 / halfHSide;
		cull.p11 = 1 / halfVSide;

		if (!is_for_shadow) {
			if (pass == Phase::Pass1) {
				cull.view = prev_view;
				prev_view = vs.view;	// pass 1, use last view
			}
			else {
				cull.view = vs.view;	// in pass 2, use current view
			}
		}
		cull.cascade_extent = frustum.ortho_max_extent * 2.0;
		cull_data->upload(&cull, sizeof(CullData));
	}

	//zero_instances_in_this(multidraw_buffer->get_internal_handle(), cmd_mats.size());	// clear instances to 0, so they can be incremented


	zero_instances_in_this(input.cmd_buf->get_internal_handle(),input.num_cmds);


	auto& device = draw.get_device();
	if (is_for_shadow)
		device.set_shader(cull_compute_cascade);
	else
		device.set_shader(cull_compute);
	const int co_size = cull.num_objects;
	const int groups = glm::ceil(int(co_size) / 256.f);

	const bool is_ortho = draw.get_current_frame_vs().is_ortho;	// this check should go elsewhere? or not, ortho is pretty special case
	device.shader().set_bool("occlusion_cull", do_occlusion_culling&&!is_ortho&& !is_for_shadow);
	device.shader().set_bool("second_pass", pass==Phase::Pass2);

	device.bind_texture_ptr(0, depth_pyramid);
	glBindSampler(0, hiZSampler);
	device.shader().set_int("lod_bias", lod_bias);
	device.shader().set_int("force_lod", r_force_lod.get_integer());

	device.shader().set_float("radius_bias", radius_bias);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, input.cmd_buf->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, input.glinst_to_inst->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, input.mod_data->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, input.obj_data_buf->get_internal_handle());
	glBindBufferBase(GL_UNIFORM_BUFFER, 5, cull_data->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, vis_bitarray->get_internal_handle());

	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

	glBindSampler(0, 0);

	
	compact_draws(input);

}
#include "Framework/ArenaAllocator.h"
void GpuCullingTest::build_data(const GpuCullInput& input)
{
	GPUSCOPESTART(gpu_cull);

	cull.cpu_obj_offset = input.num_objs;
	cull.num_objects = input.num_objs;


	const int words_needed = glm::ceil(cull.num_objects / 32.f);
	vis_bitarray->upload(nullptr, words_needed*4);
	uint32 value = 0;
	glClearNamedBufferData(vis_bitarray->get_internal_handle(),
			GL_R32UI,
			GL_RED_INTEGER,
			GL_UNSIGNED_INT,
			&value);
	
	do_cull_for_scene(input, Phase::Pass1);
}
void GpuCullingTest::build_data_2(const GpuCullInput& input)
{
	// depth pyramid update
	if (update_depth_pyramid)
	{
		const auto& viewsetup = draw.current_frame_view;
		int v_w = viewsetup.width / 2;
		int v_h = viewsetup.height / 2;
		if (depth_size.x != v_w || depth_size.y != v_h)
			init_depth_pyramid(v_w, v_h);
		downsample_depth();
	}

	do_cull_for_scene(input, Phase::Pass2);

}


extern const GLenum MODEL_INDEX_TYPE_GL;
extern ConfigVar test_ignore_bake;
void GpuCullingTest::dodraw()
{
	
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

	auto actual_width = previousPow2(w);
	auto actual_height = previousPow2(h);
	actual_depth_size = { actual_width,actual_height };


	CreateTextureArgs args;
	args.num_mip_maps = Texture::get_mip_map_count(actual_width,actual_height);
	args.width = actual_width;
	args.height = actual_height;
	//previousPow2(w*2)
	//args.num_mip_maps = getImageMipLevels(actual_width, actual_height);
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::DepthPyramid;
	args.format = GraphicsTextureFormat::r32f;

	depth_pyramid = IGraphicsDevice::inst->create_texture(args);

	auto t = Texture::load("_depth_pyramid");
	t->update_specs_ptr(depth_pyramid);
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
	const int levels = Texture::get_mip_map_count(actual_depth_size.x, actual_depth_size.y);
	int width = actual_depth_size.x;
	int height = actual_depth_size.y;
			glBindSampler(0, hiZSampler);
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

void GpuCullingTest::compact_draws(const GpuCullInput& input)
{
	GLuint zero = 0;
	glClearNamedBufferData(
		input.count_buf->get_internal_handle(),
		GL_R32UI,
		GL_RED_INTEGER,
		GL_UNSIGNED_INT,
		&zero
	);


	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, input.cmd_buf->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, input.batches_buf->get_internal_handle());
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mat_buf);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, input.count_buf->get_internal_handle());

	// stored in mdi buf after first 2 sections
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, input.draw_to_batch->get_internal_handle());


	draw.set_shader(compaction);
	int groups_x = glm::ceil(input.num_cmds / 32.f);
	draw.shader().set_int("num_draws", input.num_batches);
	draw.shader().set_int("command_count", input.num_cmds);

	glDispatchCompute(groups_x, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void GpuCullingTest::zero_instances_in_this(bufferhandle mdi_buf, int count)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mdi_buf);
	draw.set_shader(zero_instances_mdi);
	int groups_x = glm::ceil(count / 256.f);
	draw.shader().set_int("draw_count", count);
	glDispatchCompute(groups_x, 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

}

void GpuCullingTest::do_shadow_cull(const GpuCullInput& input, Frustum f)
{
	do_cull(input, {},true, f);
}

