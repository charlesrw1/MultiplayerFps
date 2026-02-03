#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"
#include "Level.h"
#include "Assets/AssetRegistry.h"
#include "GameEnginePublic.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/Serializer.h"
#include <stdexcept>

#ifdef EDITOR_BUILD
// create native entities as a fake "Asset" for drag+drop and double click open to create instance abilities
class ComponentTypeMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 181, 159, 245 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Component-Entity";
	}

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const  override {

		// all editor spawnable components here fixme
		filepaths.push_back("DecalComponent");

		filepaths.push_back("SpotLightComponent");
		filepaths.push_back("PointLightComponent");
		filepaths.push_back("SunLightComponent");
		filepaths.push_back("SkylightComponent");
		filepaths.push_back("CubemapComponent");
		filepaths.push_back("LightmapComponent");

		filepaths.push_back("MeshComponent");

		filepaths.push_back("ParticleComponent");
		filepaths.push_back("TrailComponent");
		filepaths.push_back("BeamComponent");

		filepaths.push_back("SoundComponent");

		filepaths.push_back("ParticleInstComponent");

	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Component::StaticType; }

};
REGISTER_ASSETMETADATA_MACRO(ComponentTypeMetadata);
#endif

void Component::set_owner_dont_serialize_or_edit(bool b)
{
	if (get_owner())
		get_owner()->dont_serialize_or_edit = b;
}
void Component::set_ticking(bool shouldTick)
{
	auto level = eng->get_level();
	if (level && tick_enabled != shouldTick && init_state == initialization_state::CALLED_START) {
		if (shouldTick)
			level->add_to_update_list(this);
		else
			level->remove_from_update_list(this);
	}
	tick_enabled = shouldTick;
}
#include "Framework/Log.h"
void Component::serialize(Serializer& s)
{
	

}
void Component::init_updater()
{
	auto level = eng->get_level();
	ASSERT(level);
	ASSERT(init_state == initialization_state::HAS_ID);
	if (level && tick_enabled)
		level->add_to_update_list(this);
}
void Component::shutdown_updater()
{
	auto level = eng->get_level();
	ASSERT(level);
	if (level && tick_enabled)
		level->remove_from_update_list(this);
}

void Component::activate_internal_step2()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		start();
		init_updater();
	}
	init_state = initialization_state::CALLED_START;
}

void Component::deactivate_internal()
{
	// can call stop after a pre_start()
	ASSERT(init_state == initialization_state::CALLED_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		stop();
	}
	shutdown_updater();
	init_state = initialization_state::HAS_ID;
}

void Component::destroy()
{
	ASSERT(eng->get_level());
	eng->get_level()->destroy_component(this);
}
void Component::sync_render_data()
{
	// changed to start or pre_start
	//if(init_state == initialization_state::CALLED_START)
		eng->get_level()->add_to_sync_render_data_list(this);
}


void Component::destroy_internal()
{
	if(init_state==initialization_state::CALLED_START)
		deactivate_internal();
	ASSERT(entity_owner);
	entity_owner->remove_this_component_internal(this);
	ASSERT(entity_owner == nullptr);
}

Component::~Component() {
	ASSERT(init_state!=initialization_state::CALLED_START);
}

const glm::mat4& Component::get_ws_transform() {
	return get_owner()->get_ws_transform();
}