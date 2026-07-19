#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL3/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "Render/TaaManager.h"
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

void Renderer::draw_height_fog(IGraphicsTexture* target) {
	GPU_SCOPE("draw_height_fog");

	if (!r_drawfog.get_bool())
		return;
	if (scene.skylights.empty())
		return;

	if (enable_volumetric_fog.get_bool()) {

		RenderPassState state;
		auto color_info = {ColorTargetInfo(target)};
		state.color_infos = color_info;
		gfx().set_render_pass(state);

		RenderPipelineState setup;
		setup.blend = BlendState::BLEND;
		setup.depth_testing = false;
		setup.depth_writes = false;
		setup.program = get_prog_man().get_obj(prog.volfog_apply);
		setup.vao = get_empty_vao();
		get_device().set_pipeline(setup);

		gfx().bind_storage_buffer_base(4, volfog.buffer.param);

		bind_texture_ptr(0, tex.scene_depth);
		bind_texture_ptr(1, volfog.texture.volume);

		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	}

	RSkylight_Internal& skylight_int = scene.skylights.at(0);
	Render_Skylight& skylight = skylight_int.skylight;
	if (!skylight.fog_enabled)
		return;

	gpu::FogUniforms uniformsToUpload{};
	uniformsToUpload.color = color32_to_vec4(skylight.fog_color);
	uniformsToUpload.density = skylight.height_fog_density;
	uniformsToUpload.exp_falloff = skylight.height_fog_exp;
	uniformsToUpload.height = skylight.height_fog_start;
	uniformsToUpload.flags = skylight.fog_use_skylight_cubemap;

	uniformsToUpload.max_mip = skylight.fog_cubemap_max_mip;
	uniformsToUpload.min_mip_dist = skylight.fog_cubemap_min_dist;
	uniformsToUpload.max_mip_dist = skylight.fog_cubemap_max_dist;

	buf.fog_uniforms->upload(&uniformsToUpload, sizeof(uniformsToUpload));

	RenderPassState state;
	auto color_info = {ColorTargetInfo(target)};
	state.color_infos = color_info;
	gfx().set_render_pass(state);

	RenderPipelineState setup;
	setup.blend = BlendState::BLEND;
	setup.depth_testing = false;
	setup.depth_writes = false;
	setup.program = get_prog_man().get_obj(prog.height_fog);
	setup.vao = get_empty_vao();
	get_device().set_pipeline(setup);

	gfx().bind_storage_buffer_base(1, buf.fog_uniforms);
	const Texture* reflectionProbeTex = skylight.generated_cube;

	bind_texture_ptr(0, tex.scene_depth);
	bind_texture_ptr(1, reflectionProbeTex->gpu_ptr);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void Renderer::deferred_decal_pass() {
	gfx().bind_uniform_buffer_base(0, ubo.current_frame);
	decalBatcher->draw_decals();
}
void Renderer::sync_update() {
	CPU_FUNCTION();

	if (enable_vsync.was_changed())
		gfx().set_vsync(enable_vsync.get_bool());

	scene.execute_deferred_deletes();
	g_modelMgr.execute_deferred_model_frees();

	update_debug_grid(); // makes it visible/hidden

	for (auto& mbo_ : scene.meshbuilder_objs.objects) {
		auto& mbo = mbo_.type_;
		if (!mbo.obj.visible)
			continue;
		mbo.dd.init_from(*mbo.obj.meshbuilder);
	}
	for (auto& po_ : scene.particle_objs.objects) {
		auto& po = po_.type_;
		po.dd.init_from(*po.obj.meshbuilder);
	}

	// For TAA, double buffer bones

	scene.flip_bone_buffers();
	auto mgr = GameAnimationMgr::inst;
	assert(mgr);

	if (mgr->get_num_matricies_used() > scene.gpu_skinned_mats_buffer_size / 2)
		Fatalf("out of animated buffer memory\n");

	scene.gpu_skinned_mats_buffer->sub_upload(
		mgr->get_bonemat_ptr(0),
		sizeof(glm::mat4) * mgr->get_num_matricies_used(),
		scene.get_front_bone_buffer_offset() * sizeof(glm::mat4));
}
ConfigVar r_print_light_tiles("r.print_light_tiles", "0", CVAR_BOOL | CVAR_DEV, "");

void Renderer::scene_draw(SceneDrawParamsEx params, View_Setup view) {
	RENDER_SCOPE("scene_draw");

	if (view.width > 5000 || view.height > 5000 || view.height <= 4|| view.width <= 4) {
		// something went wrong
		view.width = 100;
		view.height = 100;
	}

	// glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	r_taa_manager.start_frame();
	if (r_taa_32f.was_changed()) {
		refresh_render_targets_next_frame = true;
	}

	LightCookieAtlas::inst->update();
	// matman.pre_render_update();
	spotShadows->update();
	check_cubemaps_dirty();

	const bool temp_disable_taa = view.is_ortho; // ortho view doesnt work with TAA

	if (temp_disable_taa) {
		disable_taa_this_frame = true;
	}

	// modify view_setup for TAA, fixme
	if (r_taa_enabled.get_bool() && !temp_disable_taa) {
		view.proj =
			r_taa_manager.add_jitter_to_projection(view.proj, r_taa_manager.calc_frame_jitter(view.width, view.height));
		view.viewproj = view.proj * view.view;
	}

	scene_draw_internal(params, view);
	last_frame_main_view = view;

	// swap last frame and current frame, fixme
	if (r_taa_enabled.get_bool() && !temp_disable_taa) {
		std::swap(tex.last_scene_color, tex.scene_color);
		std::swap(tex.last_scene_motion, tex.scene_motion);

		tex.scene_color_vts_handle->update_specs_ptr(tex.scene_color);
		tex.scene_motion_vts_handle->update_specs_ptr(tex.scene_motion);

		//	glNamedFramebufferTexture(fbo.forward_render, GL_COLOR_ATTACHMENT0, tex.scene_color->get_internal_handle(),
		// 0); 	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT3, tex.scene_color->get_internal_handle(), 0);
		//	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT5, tex.scene_motion->get_internal_handle(), 0);
	}

	// fixme:
	if (r_print_light_tiles.get_bool()) {
		auto& counts = lightListCuller->get_counts();
		if (!counts.empty()) {
			const float height = Canvas::calc_text_size("0").h;
			for (int y = 0; y < light_frustum_size_y; y++) {
				for (int x = 0; x < light_frustum_size_x; x++) {
					int count = counts.at(y * light_frustum_size_x + x);
					float ypos = y * (cur_h / float(light_frustum_size_y));
					float xpos = x * (cur_w / float(light_frustum_size_x));
					auto str = std::to_string(count);
					TextShape text;
					text.with_drop_shadow = true;
					text.color = COLOR_WHITE;
					text.rect.x = xpos;
					text.rect.y = ypos + height;
					text.text = str;
					text.drop_shadow_ofs = 1;
					UiSystem::inst->window.draw(text);
				}
			}
		}
	}
}

void get_view_mat(int idx, glm::vec3 pos, glm::mat4& view, glm::vec3& front);

void Renderer::update_cubemap_specular_irradiance(glm::vec3 ambientCube[6], Texture* cubemap, glm::vec3 position,
												  bool skybox_only) {
	const int specular_cubemap_size = EnviornmentMapHelper::CUBEMAP_SIZE;
	const int num_mips = Texture::get_mip_map_count(specular_cubemap_size, specular_cubemap_size);
	assert(cubemap);
	// static Texture* somthing = nullptr;
	if (!cubemap->gpu_ptr) { // not created yet
		CreateTextureArgs args;
		args.format = GraphicsTextureFormat::rgb16f;
		args.type = GraphicsTextureType::tCubemap;
		args.sampler_type = GraphicsSamplerType::CubemapDefault;
		args.num_mip_maps = num_mips;
		args.width = args.height = specular_cubemap_size;
		cubemap->gpu_ptr = gfx().create_texture(args);
		// glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap->gl_id);
		// glTextureStorage2D(cubemap->gl_id, num_mips, GL_RGB16F, specular_cubemap_size, specular_cubemap_size);
		// glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		// glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		// glTextureParameteri(cubemap->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		// glTextureParameteri(cubemap->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//	cubemap->width = cubemap->height = specular_cubemap_size;

		// auto somthing = Texture::install_system("_TEST");
		// somthing->update_specs(cubemap->gl_id, CUBEMAP_SIZE, CUBEMAP_SIZE, 3, {});
		// somthing->type = Texture_Type::TEXTYPE_2D;

		// glCreateTextures(GL_TEXTURE_2D, 1, &somthing->gl_id);
		// glTextureStorage2D(somthing->gl_id, 1, GL_RGB16F, 512, 512);
		// set_default_parameters(somthing->gl_id);
		// somthing->width = somthing->height = 512;
		// somthing->type = Texture_Type::TEXTYPE_2D;
	}

	auto& helper = EnviornmentMapHelper::get();

	for (int i = 0; i < 6; i++) {
		glm::mat4 viewmat;
		glm::vec3 viewfront;
		get_view_mat(i, position, viewmat, viewfront);
		View_Setup cubemap_view(viewmat, glm::radians(90.f), 0.01, 100.f, specular_cubemap_size, specular_cubemap_size);

		SceneDrawParamsEx params(0.f, 0.016f);
		params.draw_ui = false;
		params.draw_world = true;
		params.is_editor = false;
		params.output_to_screen = false;
		params.is_cubemap_view = true;
		params.skybox_only = skybox_only;

		scene_draw_internal(params, cubemap_view);

		get_device().set_depth_write_enabled(true); // blit_textures path requires depth-mask on

		// set cubemap texture to a temp framebuffer
		// glNamedFramebufferTextureLayer(cubemap_fbo, GL_COLOR_ATTACHMENT0, cubemap->gl_id, 0/* highest mip*/, i/* face
		// index*/);
		////glNamedFramebufferTexture(cubemap_fbo, GL_COLOR_ATTACHMENT0, somthing->gl_id, 0);
		//// blit output to framebuffer
		// glBlitNamedFramebuffer(fbo.forward_render, cubemap_fbo,
		//	0, 0, specular_cubemap_size, specular_cubemap_size,
		//	0, 0, specular_cubemap_size, specular_cubemap_size,
		//	GL_COLOR_BUFFER_BIT,
		//	GL_NEAREST);

		GraphicsBlitInfo blit;
		blit.src.texture = tex.scene_color;
		blit.dest.texture = cubemap->gpu_ptr;
		blit.dest.mip = 0;
		blit.dest.layer = i; // face index
		blit.src.x = blit.src.y = blit.dest.x = blit.dest.y = 0;
		blit.src.w = blit.src.h = blit.dest.w = blit.dest.h = specular_cubemap_size;
		gfx().blit_textures(blit);
	}

	//	glDeleteFramebuffers(1, &cubemap_fbo);

	helper.compute_specular_new(cubemap);
	helper.compute_irradiance_new(cubemap, ambientCube);
}

void Renderer::check_cubemaps_dirty() {
	GPU_FUNCTION();

	bool had_changes = false;
	double start = GetTime();
	if (!scene.skylights.empty() && (scene.skylights[0].skylight.wants_update || force_render_cubemaps.get_bool())) {
		sys_print(Debug, "check_cubemaps_dirty:rendering skylight cubemap\n");
		auto& skylight = scene.skylights[0];
		update_cubemap_specular_irradiance(skylight.ambientCube, (Texture*)skylight.skylight.generated_cube,
										   glm::vec3(0.f), true);
		skylight.skylight.wants_update = false;

		auto up = colorvec_linear_to_srgb(glm::vec4(skylight.ambientCube[2], 0.0));
		auto down = colorvec_linear_to_srgb(glm::vec4(skylight.ambientCube[3], 0.0));

		// sys_print(Debug, "skylight cubemap up/down irrad: (%f %f %f) (%f %f %f)\n", up.x, up.y, up.z, down.x, down.y,
		// down.z);
		had_changes = true;
	}
	RenderGiManager::inst->render_frame_tick();
	force_render_cubemaps.set_bool(false);

	if (had_changes) {
		double now = GetTime();
		sys_print(Debug, "Renderer::check_cubemaps_dirty: time %f\n", float(now - start));
	}
}
ConfigVar r_no_postprocess("r.skip_pp", "0", CVAR_BOOL | CVAR_DEV, "disable post processing");
ConfigVar r_devicecycle("r.devicecycle", "0", CVAR_INTEGER | CVAR_DEV, "", 0, 10);
ConfigVar r_taa_blend("r.taa_blend", "0.75", CVAR_FLOAT, "", 0, 1.0);
ConfigVar r_taa_stationary_blend("r.taa_stationary_blend", "0.95", CVAR_FLOAT, "TAA history blend for stationary pixels", 0, 0.99);
ConfigVar r_taa_motion_blend("r.taa_motion_blend", "0.85", CVAR_FLOAT, "TAA history blend for moving pixels", 0, 0.99);
ConfigVar r_taa_adaptive_blend("r.taa_adaptive_blend", "0", CVAR_BOOL, "use velocity-adaptive blend instead of fixed amt");
ConfigVar r_taa_sharpness("r.taa_sharpness", "0.0", CVAR_FLOAT, "TAA post-blend sharpening strength", 0, 3.0);
ConfigVar r_taa_flicker_remove("r.taa_flicker_remove", "1", CVAR_BOOL, "");
ConfigVar r_taa_reproject("r.taa_reproject", "0", CVAR_BOOL, "");
ConfigVar r_taa_dilate_velocity("r.taa_dilate_velocity", "0", CVAR_BOOL, "");
float taa_doc_mult = 80.0;
float taa_doc_vel_bias = 0.001;
float taa_doc_bias = 0.2;
float taa_doc_pow = 0.15;

void taa_menu() {
	float sb = r_taa_stationary_blend.get_float();
	float mb = r_taa_motion_blend.get_float();
	float sh = r_taa_sharpness.get_float();
	if (ImGui::DragFloat("stationary_blend", &sb, 0.01f, 0.f, 0.99f)) r_taa_stationary_blend.set_float(sb);
	if (ImGui::DragFloat("motion_blend", &mb, 0.01f, 0.f, 0.99f)) r_taa_motion_blend.set_float(mb);
	if (ImGui::DragFloat("sharpness", &sh, 0.01f, 0.f, 3.0f)) r_taa_sharpness.set_float(sh);
	ImGui::DragFloat("taa_doc_mult", &taa_doc_mult, 0.1, 1, 100);
	ImGui::DragFloat("taa_doc_vel_bias", &taa_doc_vel_bias, 0.001, 0.0001, 0.01);
	ImGui::DragFloat("taa_doc_bias", &taa_doc_bias, 0.01, 0.001, 0.2);
	ImGui::DragFloat("taa_doc_pow", &taa_doc_pow, 0.01, 0, 1);
}
ADD_TO_DEBUG_MENU(taa_menu);
static glm::vec3 pp_color_tint = glm::vec3(1.f);

void post_process_menu() {
	if (!PPManager::inst) return;
	// Display the currently-active blended settings (read-only in debug menu)
	auto pp = PPManager::inst->get_active();
	ImGui::Text("tonemap_type: %d", pp.tonemap_type);
	ImGui::Text("contrast:     %.3f", pp.contrast);
	ImGui::Text("saturation:   %.3f", pp.saturation);
	ImGui::Text("exposure:     %.3f", pp.exposure);
	ImGui::Text("bloom:        %s  intensity %.4f", pp.bloom_enabled ? "on" : "off", pp.bloom_intensity);
}
ADD_TO_DEBUG_MENU(post_process_menu);

void Renderer::upload_light_and_decal_buffers() {
	GPU_SCOPE("upload_light_and_decal_buffers");

	auto upload_light_data = [&]() {
		using glu = gpu::LightingObjectUniforms;
		ArenaScope memScope(get_arena());
		const int num_lights = scene.light_list.objects.size();
		glu* lights_buffer = get_arena().alloc_bottom_type<glu>(num_lights);

		int index = 0;
		for (auto& light_pair : scene.light_list.objects) {
			auto& light = light_pair.type_.light;

			glm::mat4 ModelTransform = glm::translate(glm::mat4(1.f), light.position);
			const float scale = light.radius;
			ModelTransform = glm::scale(ModelTransform, glm::vec3(scale));

			glu& light_uniforms = lights_buffer[index];
			light_uniforms.transform = ModelTransform;
			light_uniforms.position_radius = vec4(light.position, light.radius);
			light_uniforms.flags = light.is_spotlight;

			const bool casts_shadow = light.casts_shadow_mode != 0 && light_pair.type_.shadow_array_handle != -1;
			const bool has_cookie = light.projected_texture != nullptr;
			light_uniforms.flags |= int(casts_shadow) << 1;
			light_uniforms.flags |= int(has_cookie) << 2;
			light_uniforms.spot_inner = cos(glm::radians(light.conemin));
			light_uniforms.spot_angle = cos(glm::radians(light.conemax));
			light_uniforms.spot_normal = vec4(light.normal, 0);
			light_uniforms.epsilon = shadowmap.tweak.epsilon * 0.03f;
			light_uniforms.light_color = vec4(light.color, 0);
			light_uniforms.cookieAtlas = light_pair.type_.cookie_atlas;
			if (casts_shadow) {
				light_uniforms.lighting_view_proj = light_pair.type_.lightViewProj;
				Rect2d rect = spotShadows->get_atlas().get_atlas_rect(light_pair.type_.shadow_array_handle);
				glm::ivec2 atlas_size = spotShadows->get_atlas().get_size();
				// xy is scale, zw is offset
				glm::vec4 as_vec4 = glm::vec4(float(rect.w) / atlas_size.x, float(rect.h) / atlas_size.y,
											  float(rect.x) / atlas_size.x, float(rect.y) / atlas_size.y);
				light_uniforms.atlas_offset = as_vec4;
			}
			index += 1;
		}
		buf.lighting_uniforms->upload(lights_buffer, num_lights * sizeof(glu));
	};
	upload_light_data();

	auto upload_decal_data = [&]() {
		using gdu = gpu::DecalObjectUniforms;
		ArenaScope memScope(get_arena());
		const int num_decals = scene.decal_list.objects.size();
		gdu* decal_buffer = get_arena().alloc_bottom_type<gdu>(num_decals);

		for (int i = 0; i < scene.decal_list.objects.size(); i++) {
			auto& obj = scene.decal_list.objects[i].type_.decal;
			if (!obj.material)
				continue;
			MaterialInstance* l = (MaterialInstance*)obj.material;
			if (l->get_master_material()->usage != MaterialUsage::Decal)
				continue;
			glm::mat4 ModelTransform = obj.transform;
			auto invTransform = glm::inverse(ModelTransform);
			gdu& decal_obj = decal_buffer[i];
			decal_obj.uv_scale_x = obj.uv_scale.x;
			decal_obj.uv_scale_y = obj.uv_scale.y;
			decal_obj.fs_mat_id = l->impl->gpu_buffer_offset;
			decal_obj.transform = ModelTransform;
			decal_obj.inv_transform = invTransform;
		}
		buf.decal_uniforms->upload(decal_buffer, num_decals * sizeof(gdu));
	};
	upload_decal_data();

	decalBatcher->build_batches();

	lightListCuller->cull(current_frame_view);
}

ConfigVar enable_ssr("r.ssr", "1", CVAR_BOOL, "");
ConfigVar dont_attach_velocity("r.dont_attach_velocity", "0", CVAR_BOOL, "");
