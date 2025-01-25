#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"
#include "Level.h"

CLASS_IMPL(EntityComponent);


void EntityComponent::destroy()
{
	ASSERT(eng->get_level());
	eng->get_level()->destroy_component(this);
}


void EntityComponent::initialize_internal_step1()
{
	if(!get_owner()->get_start_disabled() || eng->is_editor_level())
		activate_internal_step1();
}
void EntityComponent::initialize_internal_step2()
{
	if(!get_owner()->get_start_disabled() || eng->is_editor_level())
		activate_internal_step2();
}

void EntityComponent::destroy_internal()
{
	if(init_state==initialization_state::CALLED_START)
		deactivate_internal();
	ASSERT(entity_owner);
	entity_owner->remove_this_component_internal(this);
	ASSERT(entity_owner == nullptr);
}

EntityComponent::~EntityComponent() {
	ASSERT(init_state != initialization_state::CALLED_PRE_START && init_state!=initialization_state::CALLED_START);
}

const glm::mat4& EntityComponent::get_ws_transform() {
	return get_owner()->get_ws_transform();
}