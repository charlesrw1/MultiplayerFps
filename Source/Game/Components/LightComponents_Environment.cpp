// Environment lighting components: Skylight, Cubemap, GiVolume, AreaishLight

#include "LightComponents.h"
#include "Render/DrawPublic.h"
#include "Render/Render_Volumes.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/Model.h"

#include "BillboardComponent.h"
#include "MeshComponent.h"
#include "Game/Components/MeshbuilderComponent.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "GameEnginePublic.h"
#include "Level.h"

////
// SkylightComponent
////

void SkylightComponent::start() {
	ASSERT(get_owner() != nullptr);
	mytexture = new Texture; // g_imgs.install_system_texture("_skylight");
	sync_render_data();

	if (eng->is_editor_level()) {
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/_nearest/skylight.png"));
		b->dont_serialize_or_edit = true; // editor only item, dont serialize
	}
}
void SkylightComponent::stop() {
	ASSERT(idraw != nullptr);
	idraw->get_scene()->remove_skylight(handle);
	// delete mytexture;
	mytexture = nullptr;
}
void SkylightComponent::on_sync_render_data() {
	ASSERT(idraw != nullptr);
	bool wants_recapture = !handle.is_valid();
	wants_recapture |= recapture_skylight.check_and_swap();

	if (!handle.is_valid())
		handle = idraw->get_scene()->register_skylight();
	Render_Skylight sl;
	sl.generated_cube = mytexture;
	sl.wants_update = wants_recapture;
	sl.fog_enabled = enable_fog;
	sl.height_fog_density = fog_density;
	sl.height_fog_exp = fog_height_falloff;
	sl.height_fog_start = fog_height_start;
	sl.fog_color = fog_color;
	sl.fog_use_skylight_cubemap = use_sky_cubemap_for_fog;
	sl.fog_cubemap_min_dist = fog_cubemap_min_dist;
	sl.fog_cubemap_max_dist = fog_cubemap_max_dist;
	sl.fog_cubemap_max_mip = fog_cubemap_max_mip;

	idraw->get_scene()->update_skylight(handle, sl);
}

#ifdef EDITOR_BUILD
void SkylightComponent::editor_on_change_property() {
	ASSERT(idraw != nullptr);
	sync_render_data();
}
#endif

////
// CubemapComponent
////

void CubemapComponent::start() {
	ASSERT(get_owner() != nullptr);
	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = true;
		{
			auto mesh = get_owner()->create_component<MeshComponent>();
			mesh->set_ignore_baking(true);
			mesh->set_ignore_cubemap_view(true);
			mesh->dont_serialize_or_edit = true;
			mesh->set_model(Model::load("cube1m.cmdl"));
			mesh->set_material_override(MaterialInstance::load("cubemap_zone.mi"));
		}
		auto meshEntity = eng->get_level()->spawn_entity();
		auto mesh = meshEntity->create_component<MeshComponent>();
		mesh->set_ignore_baking(true);
		mesh->set_ignore_cubemap_view(true);
		mesh->set_model(Model::load("eng/LIGHT_SPHERE.cmdl"));
		mesh->set_material_override(MaterialInstance::load("top_down/gray_metal.mi"));
		meshEntity->dont_serialize_or_edit = true;
		this->editor_mesh = meshEntity;

		update_editormeshbuilder();
	}
	on_changed_transform();

	sync_render_data();
}
void CubemapComponent::on_sync_render_data() {}

void CubemapComponent::update_editormeshbuilder() {
	ASSERT(true); // called unconditionally, guard inside
	if (!editor_meshbuilder)
		return;
	glm::vec3 scale = get_owner()->get_ls_scale();
	auto boxmin = vec3(-0.5f);
	auto boxmax = vec3(0.5f);
	editor_meshbuilder->mb.Begin();
	editor_meshbuilder->mb.PushLineBox(boxmin, boxmax, COLOR_GREEN);
	editor_meshbuilder->mb.End();
}
void CubemapComponent::stop() {
	ASSERT(true); // editor_meshbuilder may be null in non-editor builds
	// delete mytexture;
	if (editor_meshbuilder) {
		editor_meshbuilder->destroy();
		editor_meshbuilder = nullptr;
	}
	if (editor_mesh.get()) {
		editor_mesh.get()->destroy();
	}
}

void CubemapComponent::on_changed_transform() {
	ASSERT(true); // editor_mesh may not exist
	update_editormeshbuilder();
	if (editor_mesh.get()) {
		editor_mesh->set_ws_position(get_ws_position());
	}
}

#ifdef EDITOR_BUILD
void CubemapComponent::editor_on_change_property() {
	ASSERT(true); // recapture may or may not be set
	if (recapture.check_and_swap()) {
		sync_render_data();
	}
	update_editormeshbuilder();
}
#endif

////
// GiVolumeComponent
////

void GiVolumeComponent::start() {
	ASSERT(get_owner() != nullptr);
	if (eng->is_editor_level()) {
		auto mesh = get_owner()->create_component<MeshComponent>();
		mesh->set_ignore_baking(true);
		mesh->set_ignore_cubemap_view(true);
		mesh->set_model(Model::load("cube1m.cmdl"));
		mesh->set_material_override(MaterialInstance::load("giprobe_zone.mi"));
		mesh->dont_serialize_or_edit = true;
	}
}

void GiVolumeComponent::stop() {}

////
// AreaishLightComponent
////

void AreaishLightComponent::start() {
	ASSERT(get_owner() != nullptr);
	mat = imaterials->create_dynmaic_material(MaterialInstance::load("emissive_basic.mm"));
	auto mesh = get_owner()->create_component<MeshComponent>();
	mesh->set_model(get_model_to_use());
	mesh->set_material_override(mat.get());
	mat->set_floatvec_parameter("EmissiveColor", color32_to_vec4(color) * intensity);
}

void AreaishLightComponent::stop() {}

void AreaishLightComponent::editor_on_change_property() {
	ASSERT(mat != nullptr);
	glm::vec4 linear = colorvec_srgb_to_linear(color32_to_vec4(color));
	mat->set_floatvec_parameter("EmissiveColor", linear * intensity);
	get_owner()->get_component<MeshComponent>()->set_model(get_model_to_use());
}

Model* AreaishLightComponent::get_model_to_use() {
	ASSERT(true); // override_model may be null; falls back to default
	if (override_model)
		return override_model;
	return Model::load("eng/plane.cmdl");
}
