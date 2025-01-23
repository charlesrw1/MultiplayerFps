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

void EntityComponent::initialize_internal()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		start();
		init_updater();
	}
	init_state = initialization_state::INITIALIZED;
}
void EntityComponent::destroy_internal()
{
	ASSERT(init_state == initialization_state::INITIALIZED);
	ASSERT(entity_owner);

	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		end();
		shutdown_updater();
	}

	entity_owner->remove_this_component_internal(this);
	ASSERT(entity_owner == nullptr);

	init_state = initialization_state::CONSTRUCTOR;
}

EntityComponent::~EntityComponent() {
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
}

const glm::mat4& EntityComponent::get_ws_transform() {
	return get_owner()->get_ws_transform();
}