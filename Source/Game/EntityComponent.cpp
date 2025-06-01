#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"
#include "Level.h"
#include "Assets/AssetRegistry.h"
#include "GameEnginePublic.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/ReflectionMacros.h"

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
		return "Component";
	}

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const  override {
		auto subclasses = ClassBase::get_subclasses<Component>();
		for (; !subclasses.is_end(); subclasses.next()) {
			if (subclasses.get_type()->allocate) {
				std::string path = subclasses.get_type()->classname;

				filepaths.push_back(path);
			}
		}
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Component::StaticType; }
	virtual bool assets_are_filepaths() const { return false; }
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
void Component::init_updater()
{
	auto level = eng->get_level();
	ASSERT(level);
	ASSERT(init_state == initialization_state::CALLED_PRE_START);
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
void Component::activate_internal_step1()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		pre_start();
	}
	init_state = initialization_state::CALLED_PRE_START;
}
void Component::activate_internal_step2()
{
	ASSERT(init_state == initialization_state::CALLED_PRE_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		start();
		init_updater();
	}
	init_state = initialization_state::CALLED_START;
}

void Component::deactivate_internal()
{
	ASSERT(init_state == initialization_state::CALLED_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		end();
		shutdown_updater();
	}
	init_state = initialization_state::HAS_ID;
}

void Component::destroy()
{
	ASSERT(eng->get_level());
	eng->get_level()->destroy_component(this);
}
void Component::sync_render_data()
{
	if(init_state!=initialization_state::CONSTRUCTOR)
		eng->get_level()->add_to_sync_render_data_list(this);
}

void Component::initialize_internal_step1()
{
	if(!get_owner()->get_start_disabled() || eng->is_editor_level())
		activate_internal_step1();
}
void Component::initialize_internal_step2()
{
	if(!get_owner()->get_start_disabled() || eng->is_editor_level())
		activate_internal_step2();
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
	ASSERT(init_state != initialization_state::CALLED_PRE_START && init_state!=initialization_state::CALLED_START);
}

const glm::mat4& Component::get_ws_transform() {
	return get_owner()->get_ws_transform();
}