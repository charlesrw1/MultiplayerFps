#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"
#include "Level.h"

CLASS_IMPL(EntityComponent);
CLASS_IMPL(EmptyComponent);


#if 0
void EntityComponent::attach_to_parent(EntityComponent* parent_component)
{
	ASSERT(parent_component);

	// prevents circular parent creations
	// checks the node we are parenting to's tree to see if THIS is one of the parent nodes
	EntityComponent* cur_node = parent_component;
	while (cur_node) {

		if (cur_node->get_parent_component() == this) {
			ASSERT(parent_comp);
			remove_this(cur_node);
			cur_node->attach_to_parent(parent_comp);
			break;
		}
		cur_node = cur_node->parent_comp;
	}

	unlink_from_parent();

	parent_component->children.push_back(this);
	parent_comp = parent_component;
	//attached_bone_name = point;
}
#endif

void EntityComponent::destroy()
{
	ASSERT(eng->get_level());
	eng->get_level()->destroy_component(this);
}

void EntityComponent::initialize_internal()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	on_init();
	init_updater();
	init_state = initialization_state::INITIALIZED;
}
void EntityComponent::destroy_internal()
{
	ASSERT(init_state == initialization_state::INITIALIZED);
	ASSERT(entity_owner);

	on_deinit();
	shutdown_updater();

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