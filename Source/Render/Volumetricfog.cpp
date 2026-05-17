#include "DrawLocal.h"
#include "imgui.h"

#include "GameEnginePublic.h"
#include "IGraphicsDevice.h"

static const ivec3 volfog_sizes[] = {{0, 0, 0}, {160, 90, 128}, {80, 45, 64}};

struct Vfog_Light
{
	glm::vec4 position_type;
	glm::vec4 color;
	glm::vec4 direction_coneangle;
};

static_assert(sizeof(gpu::VfogParams) == 144, "std140");

static IGraphicsTexture* make_volfog_volume(glm::ivec3 size) {
	CreateTextureArgs args;
	args.type = GraphicsTextureType::t3D;
	args.format = GraphicsTextureFormat::rgba16f;
	args.width = size.x;
	args.height = size.y;
	args.depth_3d = size.z;
	args.num_mip_maps = 1;
	args.sampler_type = GraphicsSamplerType::LinearClamped;
	return gfx().create_texture(args);
}

void Volumetric_Fog_System::init() {
	if (quality == 0)
		return;

	voltexturesize = volfog_sizes[1];

	texture.volume = make_volfog_volume(voltexturesize);
	texture.last_volume = make_volfog_volume(voltexturesize);

	CreateBufferArgs bargs;
	bargs.size = sizeof(gpu::VfogParams);
	bargs.flags = BUFFER_USE_DYNAMIC;
	buffer.param = gfx().create_buffer(bargs);
}

void vfogmenu() {
	ImGui::InputFloat("den", &draw.volfog.density);
	ImGui::InputFloat("falloff", &draw.volfog.falloff);
	ImGui::InputFloat("ofs", &draw.volfog.offset);

	ImGui::InputFloat("ani", &draw.volfog.anisotropy);
	ImGui::InputFloat("spread", &draw.volfog.spread);
}
ADD_TO_DEBUG_MENU(vfogmenu);
void Volumetric_Fog_System::compute() {
	if (!enable_volumetric_fog.get_bool())
		return;

	GPUFUNCTIONSTART;

	gpu::VfogParams params{};

	const auto& last_vs = draw.last_frame_main_view;

	params.volumesize = glm::ivec4(voltexturesize, 0);
	params.spread_frustumend = vec4(spread, frustum_end, 0, 0);
	params.last_viewproj = last_vs.viewproj;
	params.reprojection = vec4(temporal_sequence, 0.1, 0, 0);
	params.density_ani = glm::vec4(draw.volfog.density, draw.volfog.anisotropy, falloff, offset);
	params.num_lights = (int)draw.scene.light_list.objects.size();

	buffer.param->sub_upload(&params, sizeof(params), 0);

	gfx().begin_compute_pass();
	gfx().bind_uniform_buffer_base(4, buffer.param);
	gfx().bind_uniform_buffer_base(0, draw.ubo.current_frame);
	gfx().bind_storage_buffer_base(3, draw.buf.lighting_uniforms);

	ivec3 groups = ceil(vec3(voltexturesize) / vec3(8, 8, 1));
	{
		{ RenderPipelineState ps; ps.program = draw.get_prog_man().get_obj(prog.lightcalc); gfx().set_pipeline(ps); }

		draw.bind_texture_ptr(0, texture.last_volume);

		gfx().bind_image_for_compute(2, texture.volume, 0, -1, GraphicsImageAccess::WriteOnly);
		draw.bind_texture_ptr(1, draw.spotShadows->get_atlas().get_atlas_texture());
		extern void set_shit_fuck();
		set_shit_fuck();
		draw.bind_texture_ptr(4, draw.ddgi->probe_irradiance);
		draw.bind_texture_ptr(5, draw.ddgi->probe_depth);
		gfx().bind_storage_buffer_base(15, draw.ddgi->ddgi_probe_avg_value);

		gfx().dispatch_compute(groups.x, groups.y, groups.z);
		gfx().memory_barrier(BARRIER_SHADER_IMAGE_ACCESS);
	}
	{
		{ RenderPipelineState ps; ps.program = draw.get_prog_man().get_obj(prog.raymarch); gfx().set_pipeline(ps); }

		gfx().bind_image_for_compute(5, texture.last_volume, 0, -1, GraphicsImageAccess::WriteOnly);
		gfx().bind_image_for_compute(2, texture.volume, 0, -1, GraphicsImageAccess::ReadOnly);

		gfx().dispatch_compute(groups.x, groups.y, 1);
		gfx().memory_barrier(BARRIER_SHADER_IMAGE_ACCESS);

		// swap, rendering with voltexture
		std::swap(texture.volume, texture.last_volume);
	}

	temporal_sequence = (temporal_sequence + 1) % 16;
}
