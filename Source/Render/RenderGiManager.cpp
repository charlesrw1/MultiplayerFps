#include "Render/RenderGiManager.h"
extern ConfigVar is_editor_app;

RenderGiManager::RenderGiManager()
{
	if (is_editor_app.get_bool()) {
		CreateTextureArgs args;
		args.width = CUBEMAP_WIDTH;
		args.height = CUBEMAP_WIDTH;
		args.type = GraphicsTextureType::tCubemapArray;
		args.format = GraphicsTextureFormat::rgb16f;
		args.num_mip_maps = Texture::get_mip_map_count(CUBEMAP_WIDTH, CUBEMAP_WIDTH);
		args.sampler_type = GraphicsSamplerType::CubemapDefault;
		args.depth_3d = MAX_CUBEMAPS * 6;
		editable_cubemap_array = IGraphicsDevice::inst->create_texture(args);

		auto cm = Texture::install_system("_cubemaps");
		cm->update_specs_ptr(editable_cubemap_array);
		cm->type = TEXTYPE_CUBEMAP_ARRAY;
	}

	cubemap_volume_buffer = IGraphicsDevice::inst->create_buffer({});

	dummy_temp_cubemap = new Texture;
	
}

inline glm::vec3 get_probe_position_from_volume(const R_CubemapVolume& vol)
{
	return vol.transform[3];
}
extern ConfigVar force_render_cubemaps;
RenderGiManager* RenderGiManager::inst = nullptr;
void RenderGiManager::render_frame_tick()
{
	if (force_render_cubemaps.get_bool())
		bake_all_cubemaps();

	if (!bake_these_cubemaps.empty() || wants_bake_all_cubemaps) {
		sys_print(Info, "baking cubemaps...\n");
		for (int i = 0; i < cm_volumes.size(); i++) {
			if (SetUtil::contains(bake_these_cubemaps, i) || wants_bake_all_cubemaps) {

				sys_print(Debug, "check_cubemaps_dirty:rendering reflection vol cubemap\n");
				glm::vec3 dummy[6];
				const glm::vec3 pos = get_probe_position_from_volume(cm_volumes[i]);
				draw.update_cubemap_specular_irradiance(dummy, dummy_temp_cubemap, pos, false);
				ASSERT(dummy_temp_cubemap && dummy_temp_cubemap->gpu_ptr);

				// copy from texture to cubemap array
				const int volhandle = i;	// the index
				const int mips = Texture::get_mip_map_count(CUBEMAP_WIDTH, CUBEMAP_WIDTH);
				for (int face = 0; face < 6; face++) {
					int width = CUBEMAP_WIDTH;
					for (int mip = 0; mip < mips; mip++) {

						//GraphicsBlitInfo blit;
						//blit.src.texture = vol.generated_cube->gpu_ptr;
						//blit.dest.texture = scene.cubemap_array;
						//blit.dest.mip = mip;
						//blit.dest.layer = 6*volhandle + face;	// face index
						//blit.src.mip = mip;
						//blit.src.layer = face;
						//blit.src.x = blit.src.y = blit.dest.x = blit.dest.y = 0;
						//blit.src.w = blit.src.h = blit.dest.w = blit.dest.h = CUBEMAP_WIDTH;
						//IGraphicsDevice::inst->blit_textures(blit);
						//glCheckError();


						glCopyImageSubData(
							dummy_temp_cubemap->gpu_ptr->get_internal_handle(), GL_TEXTURE_CUBE_MAP, mip, 0, 0, face,
							editable_cubemap_array->get_internal_handle(), GL_TEXTURE_CUBE_MAP_ARRAY, mip, 0, 0, 6 * volhandle + face,
							width, width, 1
						);
						width /= 2;
					}
				}

			}
		}
	}
	bake_these_cubemaps.clear();
	wants_bake_all_cubemaps = false;
}

void RenderGiManager::update_ddgi_volumes(const DdgiVolumeGpu& volumes)
{
}

const std::vector<DdgiVolumeGpu>& RenderGiManager::get_baked_ddgi_volumes() const
{
	// TODO: insert return statement here
}

void RenderGiManager::set_loaded_ddgi_data(BakedDdgiInputData&& input)
{
}

void RenderGiManager::bake_ddgi()
{
}

void RenderGiManager::update_cubemap_volumes(const std::vector<R_CubemapVolume>& volumes)
{
	this->cm_volumes = volumes;
	cubemap_volume_buffer->upload(cm_volumes.data(), cm_volumes.size() * sizeof(R_CubemapVolume));
	draw.ddgi->calculate_lum_for_spec();
}

void RenderGiManager::set_cubemaps_texture(const Texture* cubemaps)
{
}
