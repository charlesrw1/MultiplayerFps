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
#include <SDL2/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>
void Renderer::draw_meshbuilders() {
	if (r_no_meshbuilders.get_bool())
		return;

	auto& mbFL = scene.meshbuilder_objs;
	auto& mbObjs = scene.meshbuilder_objs.objects;
	for (auto& mbPair : mbObjs) {
		auto& mb = mbPair.type_.obj;
		if (!mb.visible)
			continue;
		auto& dd = mbPair.type_.dd;
		if (dd.num_indicies == 0) // this check ...
			continue;

		if (mb.use_background_color) {
			RenderPipelineState state;
			state.program = prog.simple_solid_color;
			state.depth_testing = mb.depth_tested;
			state.depth_writes = false;
			device.set_pipeline(state);

			shader().set_mat4("ViewProj", current_frame_view.viewproj);
			shader().set_mat4("Model", mb.transform);
			shader().set_vec4("solid_color", color32_to_vec4(mb.background_color));

			glLineWidth(3);
			dd.draw(MeshBuilderDD::LINES);
			glLineWidth(1);
		}

		RenderPipelineState state;
		state.program = prog.simple;
		state.depth_testing = mb.depth_tested;
		state.depth_writes = false;
		device.set_pipeline(state);

		shader().set_mat4("ViewProj", current_frame_view.viewproj);
		shader().set_mat4("Model", mb.transform);
		dd.draw(MeshBuilderDD::LINES);
	}
}

extern ConfigVar g_draw_grid;
extern ConfigVar g_grid_size;

static handle<MeshBuilder_Object> debug_grid_handle;

void update_debug_grid() {
	static MeshBuilder mb;
	static bool has_init = false;
	if (!has_init) {
		mb.Begin();
		for (int x = 0; x < 11; x++) {
			Color32 colorx = COLOR_WHITE;
			Color32 colorz = COLOR_WHITE;
			if (x == 5) {
				colorx = COLOR_RED;
				colorz = COLOR_BLUE;
			}
			mb.PushLine(glm::vec3(-5, 0, x - 5), glm::vec3(5, 0, x - 5), colorx);
			mb.PushLine(glm::vec3(x - 5, 0, -5), glm::vec3(x - 5, 0, 5), colorz);
		}
		mb.End();
		debug_grid_handle = idraw->get_scene()->register_meshbuilder();
		has_init = true;
	}
	MeshBuilder_Object mbo;
	mbo.use_background_color = true;
	mbo.visible = g_draw_grid.get_bool();
	mbo.meshbuilder = &mb;
	idraw->get_scene()->update_meshbuilder(debug_grid_handle, mbo);
}

const static int light_frustum_size_x = 8;
const static int light_frustum_size_y = 6;

struct FrustumPlane
{
	glm::vec3 normal;
	float distance;
};

std::array<FrustumPlane, 4> get_tile_frustum_planes(const glm::vec3& camPos, const glm::vec3& forward,
													const glm::vec3& right, const glm::vec3& up, float fovY,
													float aspect, float screenWidth, float screenHeight, int tileX,
													int tileY, float tileWidth, float tileHeight) {
	using namespace glm;

	float tanHalfFovY = tan(fovY * 0.5f);
	float tanHalfFovX = tanHalfFovY * aspect;

	float ndcMinX = ((float)(tileX * tileWidth) / (float)screenWidth) * 2.0f - 1.0f;
	float ndcMaxX = ((float)((tileX + 1) * tileWidth) / (float)screenWidth) * 2.0f - 1.0f;
	float ndcMinY = 1.0f - ((float)((tileY + 1) * tileHeight) / (float)screenHeight) * 2.0f;
	float ndcMaxY = 1.0f - ((float)(tileY * tileHeight) / (float)screenHeight) * 2.0f;

	vec3 centerDir = forward;
	vec3 cornerDirs[4];
	cornerDirs[0] = normalize(forward + right * ndcMinX * tanHalfFovX + up * ndcMinY * tanHalfFovY); // left-bottom
	cornerDirs[1] = normalize(forward + right * ndcMaxX * tanHalfFovX + up * ndcMinY * tanHalfFovY); // right-bottom
	cornerDirs[2] = normalize(forward + right * ndcMaxX * tanHalfFovX + up * ndcMaxY * tanHalfFovY); // right-top
	cornerDirs[3] = normalize(forward + right * ndcMinX * tanHalfFovX + up * ndcMaxY * tanHalfFovY); // left-top

	std::array<FrustumPlane, 4> planes;
	vec3 leftNormal = normalize(cross(cornerDirs[0], cornerDirs[3]));
	planes[0] = {leftNormal, -dot(leftNormal, camPos)};
	vec3 rightNormal = normalize(cross(cornerDirs[2], cornerDirs[1]));
	planes[1] = {rightNormal, -dot(rightNormal, camPos)};
	vec3 topNormal = normalize(cross(cornerDirs[3], cornerDirs[2]));
	planes[2] = {topNormal, -dot(topNormal, camPos)};
	vec3 bottomNormal = normalize(cross(cornerDirs[1], cornerDirs[0]));
	planes[3] = {bottomNormal, -dot(bottomNormal, camPos)};

	return planes;
}

inline bool cull_sphere_by_frustum(const std::array<FrustumPlane, 4>& planes, glm::vec4 sphere) {
	bool res = true;
	res &= dot(planes[0].normal, glm::vec3(sphere)) + planes[0].distance >= -sphere.w;
	res &= dot(planes[1].normal, glm::vec3(sphere)) + planes[1].distance >= -sphere.w;
	res &= dot(planes[2].normal, glm::vec3(sphere)) + planes[2].distance >= -sphere.w;
	res &= dot(planes[3].normal, glm::vec3(sphere)) + planes[3].distance >= -sphere.w;
	return res;
}
#include "Framework/Range.h"

LightListCuller::LightListCuller() {
	auto create_buffer = [&]() {
		CreateBufferArgs args;
		args.flags = BUFFER_USE_DYNAMIC;
		return gfx().create_buffer(args);
	};
	light_indirection = create_buffer();
	light_count_buffer = create_buffer();
	tiled_uniforms = create_buffer();
}
ConfigVar r_light_use_tiled("r.light_use_tiled", "2", CVAR_INTEGER, "", 0, 2);

void LightListCuller::draw_lights() {
	GPUFUNCTIONSTART;

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	if (r_light_use_tiled.get_integer() == 1)
		state.program = draw.prog.light_accumulation_fullscreen_tiled;
	else if (r_light_use_tiled.get_integer() == 2)
		state.program = draw.prog.light_accumulation_fullscreen_tiled2;
	else
		state.program = draw.prog.light_accumulation_fullscreen;
	state.blend = BlendState::ADD;
	state.depth_testing = false;
	state.depth_writes = false;
	draw.get_device().set_pipeline(state);
	auto& tex = draw.tex;
	draw.bind_texture_ptr(0, tex.scene_gbuffer0);
	draw.bind_texture_ptr(1, tex.scene_gbuffer1);
	draw.bind_texture_ptr(2, tex.scene_gbuffer2);
	draw.bind_texture_ptr(3, tex.scene_depth);
	draw.bind_texture_ptr(4, draw.spotShadows->get_atlas().get_atlas_texture());
	auto cookieAtlas = LightCookieAtlas::inst->get_atlas();
	if (!cookieAtlas)
		cookieAtlas = draw.white_texture;
	draw.bind_texture_ptr(5, cookieAtlas);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, draw.buf.lighting_uniforms->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, tiled_uniforms->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, light_count_buffer->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, light_indirection->get_internal_handle());

	if (r_light_use_tiled.get_integer() != 1)
		draw.shader().set_int("num_lights", draw.scene.light_list.objects.size());

	if (r_light_use_tiled.get_integer() == 2) { // MAIN PATH
		// its sometimes 2x faster than normal tiled to do this "dumb" way. okay i guess.
		// normal tiled is sometimes worse than the bruteforce naive way, this way is always at least better than brute
		// force. seems to be usually ~40% faster than normal tiled i guess the indirection of looking up "num_lights"
		// hurts compared to a constant uniform?

		// could just my AMD card (rx 480) thats being weird, curious to test on something else

		const int w = draw.get_current_frame_vs().width;
		const int h = draw.get_current_frame_vs().height;
		auto& device = draw.get_device();
		glm::ivec2 tile_count = glm::ivec2(light_frustum_size_x, light_frustum_size_y);
		glm::vec2 tile_size = glm::ceil(glm::vec2(w, h) / glm::vec2(tile_count));
		for (int y = 0; y < tile_count.y; y++) {
			for (int x = 0; x < tile_count.x; x++) {
				const int index = y * tile_count.x + x;
				const int count = counts.at(index);
				draw.shader().set_int("num_lights", count);
				const int light_offset = index * gpu::MAX_TILE_LIGHTS;
				draw.shader().set_int("light_indirect_offset", light_offset);

				const int y_to_use = tile_count.y - y - 1;
				glm::vec2 ofs = glm::floor(glm::vec2(x * tile_size.x, y_to_use * tile_size.y));
				device.set_viewport(ofs.x, ofs.y, tile_size.x, tile_size.y);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}
		}
		device.set_viewport(0, 0, w, h);
	} else { // UNUSED/OPTIONAL

		// fullscreen shader, no vao used
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
}

void LightListCuller::cull(const View_Setup& setup) {
	CPUFUNCTIONSTART;

	using namespace glm;
	auto& view = setup.view;
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 forward = -vec3(view[0][2], view[1][2], view[2][2]);

	const float tile_size_x = setup.width / float(light_frustum_size_x);
	const float tile_size_y = setup.height / float(light_frustum_size_y);
	const float aspect = float(setup.width) / setup.height;

	std::vector<int16_t> lights;

	const int total_tiles = light_frustum_size_x * light_frustum_size_y;
	const int max_lights_in_tile = gpu::MAX_TILE_LIGHTS;
	counts.resize(total_tiles);

	auto& memArena = draw.get_arena();
	ArenaScope memScope(memArena);

	int* light_index_buffer = memArena.alloc_bottom_type<int>(total_tiles * max_lights_in_tile);
	int* tile_light_count = memArena.alloc_bottom_type<int>(total_tiles);

	auto cull_volume = [&](int index_x, int index_y) {
		const int my_tile_index = index_y * light_frustum_size_x + index_x;
		const int my_light_index_index = my_tile_index * max_lights_in_tile;
		int lights_in_tile = 0;

		auto furstum_planes = get_tile_frustum_planes(setup.origin, forward, right, up, setup.fov, aspect, setup.width,
													  setup.height, index_x, index_y, tile_size_x, tile_size_y);

		auto& scene_lights = draw.scene.light_list.objects;
		int light_index = -1;
		for (auto& light_type : scene_lights) {
			light_index += 1;
			RL_Internal& light = light_type.type_;
			glm::vec4 sphere(light.light.position, light.light.radius);
			const bool in_frustum = cull_sphere_by_frustum(furstum_planes, sphere);
			if (in_frustum) {
				light_index_buffer[my_light_index_index + lights_in_tile] = light_index;
				lights_in_tile += 1;
				if (lights_in_tile >= max_lights_in_tile)
					break;
			}
		}

		tile_light_count[my_tile_index] = lights_in_tile;
		counts[my_tile_index] = lights_in_tile;
	};

	for (int y = 0; y < light_frustum_size_y; y++) {
		for (int x = 0; x < light_frustum_size_x; x++) {
			cull_volume(x, y);
		}
	}

	light_indirection->upload(light_index_buffer, total_tiles * max_lights_in_tile * sizeof(int));
	light_count_buffer->upload(tile_light_count, total_tiles * sizeof(int));

	gpu::TiledLightUniforms uniforms{};
	uniforms.tile_count_x = light_frustum_size_x;
	uniforms.tile_count_y = light_frustum_size_y;

	uniforms.inv_tile_size_x = 1.0 / tile_size_x;
	uniforms.inv_tile_size_y = 1.0 / tile_size_y;

	tiled_uniforms->upload(&uniforms, sizeof(gpu::TiledLightUniforms));
}

void Renderer::accumulate_gbuffer_lighting(bool is_cubemap_view) {
	ZoneScoped;
	GPUSCOPESTART(accumulate_gbuffer_lighting);

	const auto& view_to_use = current_frame_view;

	// RenderPassSetup setup("gbuffer-lighting", fbo.forward_render, false, false, 0, 0, view_to_use.width,
	// view_to_use.height);
	auto start_render_pass = [&]() {
		auto targets = {ColorTargetInfo(tex.scene_color)};
		RenderPassState rp;
		rp.color_infos = targets;
		gfx().set_render_pass(rp);
	};
	start_render_pass();

	// auto scope = device.start_render_pass(setup);
	const bool wants_ssao = !is_cubemap_view && enable_ssao.get_bool();
	IGraphicsTexture* const ssao_tex = (wants_ssao) ? ssao.texture.result : white_texture; // skip ssao in cubemap view
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo.current_frame);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	device.reset_states();
	if (ddgi_test.get_bool()) {
		ddgi->draw_lighting(ssao_tex, is_cubemap_view);
	} else if (!r_no_indirect.get_bool()) {
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.ambient_accumulation;
		state.blend =
			BlendState::ADD; // does a mult of (albedo+ao) with the indirect lighting already in tex.scene_color
		state.depth_testing = false;
		state.depth_writes = false;
		device.set_pipeline(state);

		if (!scene.skylights.empty()) {
			device.shader().set_vec3("sky_color", scene.skylights.at(0).ambientCube[2]);
			device.shader().set_vec3("ground_color", scene.skylights.at(0).ambientCube[3]);
		}

		bind_texture_ptr(0, tex.scene_gbuffer0);
		bind_texture_ptr(1, tex.scene_gbuffer1);
		bind_texture_ptr(2, tex.scene_gbuffer2);
		bind_texture_ptr(3, tex.scene_depth);
		bind_texture_ptr(4, ssao_tex);

		// fullscreen shader, no vao used
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	device.reset_states();

	lightListCuller->draw_lights();

	// fullscreen pass for directional light(s)
	RSunInternal* sun_internal = scene.get_main_directional_light();
	if (sun_internal) {

		RenderPipelineState state;
		state.vao = get_empty_vao();
		if (debug_sun_shadow.get_bool()) {
			state.program = prog.sunlight_accumulation_debug;
			state.blend = BlendState::OPAQUE;
		} else {
			state.program = prog.sunlight_accumulation;
			state.blend = BlendState::ADD;
		}
		state.depth_testing = false;
		state.depth_writes = false;
		device.set_pipeline(state);

		bind_texture_ptr(0, tex.scene_gbuffer0);
		bind_texture_ptr(1, tex.scene_gbuffer1);
		bind_texture_ptr(2, tex.scene_gbuffer2);
		bind_texture_ptr(3, tex.scene_depth);
		bind_texture_ptr(4, draw.shadowmap.texture.shadow_array);
		glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.ubo.info);

		shader().set_vec3("uSunDirection", sun_internal->sun.direction);
		shader().set_vec3("uSunColor", sun_internal->sun.color);
		shader().set_float("uEpsilon", sun_internal->sun.epsilon);

		// fullscreen shader, no vao used
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// const Texture* reflectionProbeTex = scene.get_reflection_probe_for_render(view_to_use.origin);
	//
	// if (reflectionProbeTex&& !is_cubemap_view) {
	//	RenderPipelineState state;
	//	state.vao = get_empty_vao();
	//	state.program = prog.reflection_accumulation;
	//	state.blend = BlendState::ADD;
	//	state.depth_testing = false;
	//	state.depth_writes = false;
	//	device.set_pipeline(state);
	//	bind_texture_ptr(4, ssao_tex);
	//	bind_texture_ptr(5, reflectionProbeTex->gpu_ptr);
	//	bind_texture(6, EnviornmentMapHelper::get().integrator.get_texture());
	//	shader().set_float("specular_ao_intensity", r_specular_ao_intensity.get_float());
	//
	//	glDrawArrays(GL_TRIANGLES, 0, 3);
	//}
}