#include "DrawLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include <glm/gtc/matrix_transform.hpp>

const static int csm_resolutions[] = { 0, 256, 512, 1024 };

void shadow_map_tweaks()
{
	auto& tweak = draw.shadowmap.tweak;
	ImGui::DragFloat("log lin", &tweak.log_lin_lerp_factor, 0.02);
	if (ImGui::SliderInt("quality", &tweak.quality, 0, 4))
		draw.shadowmap.targets_dirty = true;
	ImGui::DragFloat("epsilon", &tweak.epsilon, 0.01);
	ImGui::DragFloat("pfac", &tweak.poly_factor, 0.01);
	ImGui::DragFloat("punit", &tweak.poly_units, 0.01);
	ImGui::DragFloat("zscale", &tweak.z_dist_scaling, 0.01);



}
ConfigVar r_spotlight_shadow_fade_radius("r.spotlight_shadow_fade_radius", "5.0", CVAR_FLOAT, "dist to fade out spot light shadows", 0, 100);
ConfigVar r_spotlight_shadow_quality("r.spotlight_shadow_quality", "1", CVAR_INTEGER, "quality of spotlight shadow 0,1,2", 0, 2);
const static int spotlight_shadow_res[] = { 128,256,512 };

void CascadeShadowMapSystem::init()
{
	texture.shadow_vts_handle = Texture::install_system("_csm_shadow");
	texture.shadow_vts_handle->type = Texture_Type::TEXTYPE_2D_ARRAY;

	Debug_Interface::get()->add_hook("shadow map", shadow_map_tweaks);

	make_csm_rendertargets();
	glCreateBuffers(1, &ubo.info);
	glCreateBuffers(4, ubo.frame_view);
}
void CascadeShadowMapSystem::make_csm_rendertargets()
{
	if (tweak.quality == 0)
		return;
	tweak.quality = 3;
	csm_resolution = csm_resolutions[(int)tweak.quality];

	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture.shadow_array);
	glTextureStorage3D(texture.shadow_array, 1, GL_DEPTH_COMPONENT32F, csm_resolution, csm_resolution, 4);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTextureParameteri(texture.shadow_array, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float bordercolor[] = { 1.0,1.0,1.0,1.0 };
	glTextureParameterfv(texture.shadow_array, GL_TEXTURE_BORDER_COLOR, bordercolor);

	glCreateFramebuffers(1, &fbo.shadow);

	texture.shadow_vts_handle->update_specs(texture.shadow_array, csm_resolution, csm_resolution, 1, {});
}


static glm::vec4 CalcPlaneSplits(float near, float far, float log_lin_lerp)
{
	float zratio = far / near;
	float zrange = far - near;

	const float bias = 0.0001f;

	glm::vec4 planedistances;
	for (int i = 0; i < 4; i++)
	{
		float x = (i + 1) / 4.f;
		float log = near * pow(zratio, x);
		float linear = near + zrange * x;
		planedistances[i] = log_lin_lerp * (log - linear) + linear + bias;
	}

	return planedistances;
}
#include "Render/Render_Sun.h"
void CascadeShadowMapSystem::update()
{
	//int setting = draw.shadow_quality_setting.integer();
	//if (setting < 0) setting = 0;
	//else if (setting > 3) setting = 3;
	//draw.shadow_quality_setting.integer() = setting;

	//if (tweak.quality != setting) {
	//	tweak.quality = setting;
	//	targets_dirty = true;
	//}

	if (targets_dirty) {
		glDeleteTextures(1, &texture.shadow_array);
		glDeleteFramebuffers(1, &fbo.shadow);
		make_csm_rendertargets();
		targets_dirty = false;
	}
	if (tweak.quality == 0)
		return;

	auto directional_light = draw.scene.get_main_directional_light();

	if (!directional_light)
		return;

	auto& sun = directional_light->sun;
	tweak.max_shadow_dist = sun.max_shadow_dist;
	tweak.log_lin_lerp_factor = sun.log_lin_lerp_factor;
	tweak.max_shadow_dist = sun.max_shadow_dist;
	tweak.z_dist_scaling = sun.z_dist_scaling;
	{
		GPUSCOPESTART(CSM_SETUP);

		glm::vec3 directional_dir = sun.direction;

		const View_Setup& view = draw.current_frame_view;

		float near = view.near;
		float far = tweak.max_shadow_dist;

		split_distances = CalcPlaneSplits(near, far, tweak.log_lin_lerp_factor);
		for (int i = 0; i < MAXCASCADES; i++)
			update_cascade(i, view, directional_dir);

		struct Shadowmap_Csm_Ubo_Struct
		{
			mat4 data[4];
			vec4 near_planes;
			vec4 far_planes;
		}upload_data;

		for (int i = 0; i < 4; i++) {
			upload_data.data[i] = matricies[i];
			upload_data.near_planes[i] = nearplanes[i];
			upload_data.far_planes[i] = farplanes[i];
		}

		glNamedBufferData(ubo.info, sizeof Shadowmap_Csm_Ubo_Struct, &upload_data, GL_DYNAMIC_DRAW);
	}
	// now setup scene for rendering
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo.shadow);
	{
		GPUSCOPESTART(RENDER_CSM_LAYERS);

		auto& device = draw.get_device();
		RenderPassSetup setup("shadowmap", fbo.shadow, false, false /* clear it below */, 0, 0, csm_resolution, csm_resolution);
		auto scope = device.start_render_pass(setup);

		for (int i = 0; i < 4; i++) {

			glNamedFramebufferTextureLayer(fbo.shadow, GL_DEPTH_ATTACHMENT, texture.shadow_array, 0, i);

			device.set_viewport(0, 0, csm_resolution, csm_resolution);
			device.clear_framebuffer(true, true, 1.f/* depth value of 1.f to clear*/);


			View_Setup setup;
			setup.width = csm_resolution;
			setup.height = csm_resolution;
			setup.near = nearplanes[i];
			setup.far = farplanes[i];
			setup.viewproj = matricies[i];
			setup.view = setup.proj = mat4(1);	// unused

			Render_Level_Params params(
				setup,
				&draw.scene.cascades_rlists.at(i),
				&draw.scene.shadow_pass,
				Render_Level_Params::SHADOWMAP
			);

			params.provied_constant_buffer = ubo.frame_view[i];
			params.upload_constants = true;
			params.wants_non_reverse_z = true;

			draw.render_level_to_target(params);
		}
	}
}



static glm::vec3* GetFrustumCorners(const mat4& view, const mat4& projection)
{
	mat4 inv_viewproj = glm::inverse(projection * view);
	static glm::vec3 corners[8];
	int i = 0;
	for (int x = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			for (int z = 0; z < 2; z++) {
				vec4 ndc_coords = vec4(2 * x - 1, 2 * y - 1, 2 * z - 1, 1);
				vec4 world_space = inv_viewproj * ndc_coords;
				world_space /= world_space.w;
				corners[i++] = world_space;
			}
		}
	}

	return corners;
}


void CascadeShadowMapSystem::update_cascade(int cascade_idx, const View_Setup& view, vec3 directionalDir)
{
	float far = split_distances[cascade_idx];
	float near = (cascade_idx == 0) ? view.near : split_distances[cascade_idx - 1];
	if (tweak.fit_to_scene)
		near = view.near;

	// 7/30: this doesnt need to be zero to one, not used to rendering, just in GetFrustumnCorners
	mat4 camera_cascaded_proj = glm::perspective(
		view.fov,
		(float)view.width / view.height,
		near, far);

	// World space corners
	glm::vec3* corners = GetFrustumCorners(view.view, camera_cascaded_proj);
	vec3 frustum_center = vec3(0);
	for (int i = 0; i < 8; i++)
		frustum_center += corners[i];
	frustum_center /= 8.f;

	mat4 light_cascade_view = glm::lookAt(frustum_center - directionalDir, frustum_center, vec3(0, 1, 0));
	vec3 viewspace_min = vec3(INFINITY);
	vec3 viewspace_max = vec3(-INFINITY);
	if (tweak.reduce_shimmering)
	{
		float sphere_radius = 0.f;
		for (int i = 0; i < 8; i++) {
			float dist = glm::length(corners[i] - frustum_center);
			sphere_radius = glm::max(sphere_radius, dist);
		}
		sphere_radius = ceil(sphere_radius);
		vec3 world_max = frustum_center + vec3(sphere_radius);
		vec3 world_min = frustum_center - vec3(sphere_radius);

		vec3 v_max = light_cascade_view * vec4(world_max, 1.0);
		vec3 v_min = light_cascade_view * vec4(world_min, 1.0);
		viewspace_max = glm::max(v_max, v_min);
		viewspace_min = glm::min(v_max, v_min);
		viewspace_max = vec3(sphere_radius);
		viewspace_min = -viewspace_max;
	}
	else {
		for (int i = 0; i < 8; i++) {
			vec3 viewspace_corner = light_cascade_view * vec4(corners[i], 1.0);
			viewspace_min = glm::min(viewspace_min, viewspace_corner);
			viewspace_max = glm::max(viewspace_max, viewspace_corner);
		}

		// insert scaling for pcf filtering here

	}
	if (viewspace_min.z < 0)
		viewspace_min.z *= tweak.z_dist_scaling;
	else
		viewspace_min.z /= tweak.z_dist_scaling;

	if (viewspace_max.z < 0)
		viewspace_max.z /= tweak.z_dist_scaling;
	else
		viewspace_max.z *= tweak.z_dist_scaling;

	vec3 cascade_extent = viewspace_max - viewspace_min;

	// 7/30: reverse z update: make ortho matrix from zero to one
	mat4 light_cascade_proj = glm::orthoRH_ZO(viewspace_min.x, viewspace_max.x, viewspace_min.y, viewspace_max.y, viewspace_min.z, viewspace_max.z);
	mat4 shadow_matrix = light_cascade_proj * light_cascade_view;
	if (tweak.reduce_shimmering)
	{
		vec4 shadow_origin = vec4(0, 0, 0, 1);
		shadow_origin = shadow_matrix * shadow_origin;
		float w = shadow_origin.w;
		shadow_origin *= csm_resolution / 2.0f;

		vec4 rounded_origin = glm::round(shadow_origin);
		vec4 rounded_offset = rounded_origin - shadow_origin;
		rounded_offset *= 2.0f / csm_resolution;
		rounded_offset.z = 0;
		rounded_offset.w = 0;

		mat4 shadow_cascade_proj = light_cascade_proj;
		shadow_cascade_proj[3] += rounded_offset;

		shadow_matrix = shadow_cascade_proj * light_cascade_view;
	}


	matricies[cascade_idx] = shadow_matrix;// light_cascade_proj* light_cascade_view;
	nearplanes[cascade_idx] = viewspace_min.z;
	farplanes[cascade_idx] = viewspace_max.z;
}
